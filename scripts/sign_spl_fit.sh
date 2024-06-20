#!/bin/bash
#===============================================================================
#
#  sign_spl_fit.sh
#
#  Copyright (C) 2020-2023 by Digi International Inc.
#  All rights reserved.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License version 2 as published by
#  the Free Software Foundation.
#
#
#  Description:
#    Script for building signed uboot images using NXP CST.
#
#    The following Kconfig entries are used:
#      CONFIG_SIGN_KEYS_PATH: (mandatory) path to the CST folder by NXP with keys generated.
#      CONFIG_KEY_INDEX: (optional) key index to use for signing. Default is 0.
#      CONFIG_UNLOCK_SRK_REVOKE: (optional) instruct HAB not to protect the SRK_REVOKE OTP
#				  field so that key revocation is possible in closed devices.
#      ENABLE_ENCRYPTION: (optional) enable encryption of the images.
#      CONFIG_DEK_PATH: (mandatory if ENCRYPT is defined) path to a Data Encryption Key.
#                       If defined, the signed U-Boot image is encrypted with the
#                       given key. Supported key sizes: 128 bits.
#      CONFIG_MKIMAGE_LOG_PATH: (optional) path to the log generated when
#                               imx-boot was compiled. If not provided, a default
#                               'mkimage.log' file is expected in current directory.
#      CONFIG_FIT_HAB_LOG_PATH: (optional) path to the print_fit_hab log generated
#                               when imx-boot was compiled. If not provided, a default
#                               'mkimage-print_fit_hab.log' file is expected in current
#                               directory.
#
#===============================================================================

# Avoid parallel execution of this script
SINGLE_PROCESS_LOCK="/tmp/sign_script.lock.d"
trap 'rm -rf "${SINGLE_PROCESS_LOCK}"' INT TERM EXIT
while ! mkdir "${SINGLE_PROCESS_LOCK}" >/dev/null 2>&1; do
	sleep 1
done

SCRIPT_NAME="$(basename "${0}")"
SCRIPT_PATH="$(cd "$(dirname "${0}")" && pwd)"

if [ "${#}" != "2" ]; then
	echo "Usage: ${SCRIPT_NAME} input-unsigned-imx-boot.bin output-signed-imx-boot.bin"
	exit 1
fi

# Get enviroment variables from .config
[ -f .config ] && . .config

# External tools are used, so UBOOT_PATH has to be absolute
UBOOT_PATH="$(readlink -e "${1}")"
TARGET="${2}"

# Check arguments
if [ -z "${CONFIG_SIGN_KEYS_PATH}" ]; then
	echo "Undefined CONFIG_SIGN_KEYS_PATH";
	exit 1
fi
[ -d "${CONFIG_SIGN_KEYS_PATH}" ] || mkdir -p "${CONFIG_SIGN_KEYS_PATH}"

# Default values
[ -z "${CONFIG_KEY_INDEX}" ] && CONFIG_KEY_INDEX="0"
CONFIG_KEY_INDEX_1="$((CONFIG_KEY_INDEX + 1))"

# Get DEK key
if [ -n "${CONFIG_DEK_PATH}" ] && [ -n "${ENABLE_ENCRYPTION}" ]; then
	if [ ! -f "${CONFIG_DEK_PATH}" ]; then
		echo "DEK not found. Generating random 128 bit DEK."
		mkdir -p "$(dirname "${CONFIG_DEK_PATH}")"
		dd if=/dev/urandom of="${CONFIG_DEK_PATH}" bs=16 count=1 >/dev/null 2>&1
	fi
	dek_size="$((8 * $(stat -L -c %s "${CONFIG_DEK_PATH}")))"
	if [ "${dek_size}" != "128" ] && [ "${dek_size}" != "192" ] && [ "${dek_size}" != "256" ]; then
		echo "Invalid DEK size: ${dek_size} bits. Valid sizes are 128, 192 and 256 bits"
		exit 1
	fi
	ENCRYPT="true"
fi

SRK_KEYS="$(echo "${CONFIG_SIGN_KEYS_PATH}"/crts/SRK*crt.pem | sed s/\ /\,/g)"
CERT_CSF="$(echo "${CONFIG_SIGN_KEYS_PATH}"/crts/CSF${CONFIG_KEY_INDEX_1}*crt.pem)"
CERT_IMG="$(echo "${CONFIG_SIGN_KEYS_PATH}"/crts/IMG${CONFIG_KEY_INDEX_1}*crt.pem)"

n_commas="$(echo "${SRK_KEYS}" | grep -o "," | wc -l)"

if [ "${n_commas}" -eq 3 ] && [ -f "${CERT_CSF}" ] && [ -f "${CERT_IMG}" ]; then
	# PKI tree already exists. Do nothing
	echo "Using existing PKI tree"
elif [ "${n_commas}" -eq 0 ] || [ ! -f "${CERT_CSF}" ] || [ ! -f "${CERT_IMG}" ]; then
	# Generate PKI
	trustfence-gen-pki.sh "${CONFIG_SIGN_KEYS_PATH}"

	SRK_KEYS="$(echo "${CONFIG_SIGN_KEYS_PATH}"/crts/SRK*crt.pem | sed s/\ /\,/g)"
	CERT_CSF="$(echo "${CONFIG_SIGN_KEYS_PATH}"/crts/CSF${CONFIG_KEY_INDEX_1}*crt.pem)"
	CERT_IMG="$(echo "${CONFIG_SIGN_KEYS_PATH}"/crts/IMG${CONFIG_KEY_INDEX_1}*crt.pem)"
else
	echo "Inconsistent CST folder."
	exit 1
fi

# Path for the SRK table and eFuses file
SRK_TABLE="$(pwd)/SRK_table.bin"
SRK_EFUSES="$(pwd)/SRK_efuses.bin"

# Path to log files to parse
MKIMAGE_LOG="$(pwd)/mkimage.log"
MKIMAGE_FIT_HAB_LOG="$(pwd)/mkimage-print_fit_hab.log"
[ -n "${CONFIG_MKIMAGE_LOG_PATH}" ] && MKIMAGE_LOG="${CONFIG_MKIMAGE_LOG_PATH}"
[ -n "${CONFIG_FIT_HAB_LOG_PATH}" ] && MKIMAGE_FIT_HAB_LOG="${CONFIG_FIT_HAB_LOG_PATH}"

if [ ! -e "${MKIMAGE_LOG}" ]; then
	echo "Make log '${MKIMAGE_LOG}' does not exist."
	exit 1
fi
if [ ! -e "${MKIMAGE_FIT_HAB_LOG}" ]; then
	echo "FIT HAB log '${MKIMAGE_FIT_HAB_LOG}' does not exist."
	exit 1
fi
echo "Using make log '${MKIMAGE_LOG}'"
echo "Using FIT HAB log '${MKIMAGE_FIT_HAB_LOG}'"

# Parse spl uboot HAB block and CSF offset
spl_image_offset=$(awk '/^ image_off/ {print $2}' "${MKIMAGE_LOG}")
spl_csf_offset=$(awk '/^ csf_off/ {print $2}' "${MKIMAGE_LOG}")
spl_ram_start=$(awk '/^ spl hab block:/ {print $4}' "${MKIMAGE_LOG}")
spl_header_offset=$(awk '/^ spl hab block:/ {print $5}' "${MKIMAGE_LOG}")
spl_image_len=$(awk '/^ spl hab block:/ {print $6}' "${MKIMAGE_LOG}")

# Parse sld (Second Loader image) uboot HAB blocks and CSF offset
sld_csf_offset=$(awk '/^ sld_csf_off/ {print $2}' "${MKIMAGE_LOG}")
sld_ram_start=$(awk '/^ sld hab block:/ {print $4}' "${MKIMAGE_LOG}")
sld_header_offset=$(awk '/^ sld hab block:/ {print $5}' "${MKIMAGE_LOG}")
sld_image_len=$(awk '/^ sld hab block:/ {print $6}' "${MKIMAGE_LOG}")

# Parse fit uboot HAB blocks
result_row=$(awk '/print_fit_hab/ {print NR+1}' "${MKIMAGE_FIT_HAB_LOG}")
uboot_ram_start=$(awk -v first_row="${result_row}" 'NR==first_row+0 {print $1}' "${MKIMAGE_FIT_HAB_LOG}")
uboot_image_offset=$(awk -v first_row="${result_row}" 'NR==first_row+0 {print $2}' "${MKIMAGE_FIT_HAB_LOG}")
uboot_image_len=$(awk -v first_row="${result_row}" 'NR==first_row+0 {print $3}' "${MKIMAGE_FIT_HAB_LOG}")
dtb_ram_start=$(awk -v first_row="${result_row}" 'NR==first_row+1 {print $1}' "${MKIMAGE_FIT_HAB_LOG}")
dtb_image_offset=$(awk -v first_row="${result_row}" 'NR==first_row+1 {print $2}' "${MKIMAGE_FIT_HAB_LOG}")
dtb_image_len=$(awk -v first_row="${result_row}" 'NR==first_row+1 {print $3}' "${MKIMAGE_FIT_HAB_LOG}")
atf_ram_start=$(awk -v first_row="${result_row}" 'NR==first_row+2 {print $1}' "${MKIMAGE_FIT_HAB_LOG}")
atf_image_offset=$(awk -v first_row="${result_row}" 'NR==first_row+2 {print $2}' "${MKIMAGE_FIT_HAB_LOG}")
atf_image_len=$(awk -v first_row="${result_row}" 'NR==first_row+2 {print $3}' "${MKIMAGE_FIT_HAB_LOG}")
optee_ram_start=$(awk -v first_row="${result_row}" 'NR==first_row+3 {print $1}' "${MKIMAGE_FIT_HAB_LOG}")
optee_image_offset=$(awk -v first_row="${result_row}" 'NR==first_row+3 {print $2}' "${MKIMAGE_FIT_HAB_LOG}")
optee_image_len=$(awk -v first_row="${result_row}" 'NR==first_row+3 {print $3}' "${MKIMAGE_FIT_HAB_LOG}")

# Compute SPL decryption variables
# Dek Blob Addr = Authenticate Start Address +  SPL & DDR FW image length + CSF Padding (0x2000)
spl_dek_offset=$((spl_ram_start + spl_image_len + 0x2000))
# Decrypt Start Address = Start Address + SPL header
# Decrypt Offset = Image offset (image_off)
# Decrypt size = Image length - SPL heder
spl_decrypt_start=$((spl_ram_start + spl_image_offset))
spl_decrypt_len=$((spl_image_len - spl_image_offset))
# Compute FIT decryption variables
uboot_dtb_ram_start=${uboot_ram_start}
uboot_dtb_image_offset=${uboot_image_offset}
uboot_dtb_image_len=$((uboot_image_len + dtb_image_len))
fit_dek_blob_addr=0x40400000

# Change values to hex strings
spl_dek_offset="$(printf "0x%X" ${spl_dek_offset})"
spl_decrypt_start="$(printf "0x%X" ${spl_decrypt_start})"
spl_decrypt_len="$(printf "0x%X" ${spl_decrypt_len})"
uboot_dtb_image_len="$(printf "0x%X" ${uboot_dtb_image_len})"

# SED filter for removing TEE entries on boot artifacts without TEE
if grep -qsi "tee.*not[[:blank:]]\+found\|not[[:blank:]]\+find.*tee" "${MKIMAGE_FIT_HAB_LOG}"; then
	NO_TEE_SED_FILTER="/%atf_\(auth\|decrypt\)_start%/s/, \\\\$//g;/%optee_\(auth\|decrypt\)_start%/d"
fi

# Generate actual CSF descriptor files from templates
if [ "${ENCRYPT}" = "true" ]; then
	# SPL Encryption
	sed -e "s,%srk_table%,${SRK_TABLE},g "			\
	    -e "s,%key_index%,${CONFIG_KEY_INDEX},g"		\
	    -e "s,%cert_csf%,${CERT_CSF},g"			\
	    -e "s,%cert_img%,${CERT_IMG},g"			\
	    -e "s,%spl_auth_start%,${spl_ram_start},g"		\
	    -e "s,%spl_auth_offset%,${spl_header_offset},g"	\
	    -e "s,%spl_auth_len%,${spl_image_offset},g"		\
	    -e "s,%imx-boot_auth_path%,${UBOOT_PATH},g"		\
	    -e "s,%dek_path%,${CONFIG_DEK_PATH},g"		\
	    -e "s,%dek_len%,${dek_size},g"			\
	    -e "s,%dek_offset%,${spl_dek_offset},g"		\
	    -e "s,%spl_decrypt_start%,${spl_decrypt_start},g"	\
	    -e "s,%spl_decrypt_offset%,${spl_image_offset},g"	\
	    -e "s,%spl_decrypt_len%,${spl_decrypt_len},g"	\
	    -e "s,%imx-boot_decrypt_path%,flash-spl-enc.bin,g"	\
	"${SCRIPT_PATH}/csf_templates/encrypt_uboot_spl" > csf_spl_enc.txt

	sed -e "s,%srk_table%,${SRK_TABLE},g "			\
	    -e "s,%key_index%,${CONFIG_KEY_INDEX},g"		\
	    -e "s,%cert_csf%,${CERT_CSF},g"			\
	    -e "s,%cert_img%,${CERT_IMG},g"			\
	    -e "s,%spl_auth_start%,${spl_ram_start},g"		\
	    -e "s,%spl_auth_offset%,${spl_header_offset},g"	\
	    -e "s,%spl_auth_len%,${spl_image_len},g"		\
	    -e "s,%imx-boot_auth_path%,flash-spl-enc.bin,g"	\
	    -e "s,%dek_path%,${CONFIG_DEK_PATH},g"		\
	    -e "s,%dek_len%,${dek_size},g"			\
	    -e "s,%dek_offset%,${spl_dek_offset},g"		\
	    -e "s,%spl_decrypt_start%,${spl_decrypt_start},g"	\
	    -e "s,%spl_decrypt_offset%,${spl_image_offset},g"	\
	    -e "s,%spl_decrypt_len%,${spl_decrypt_len},g"	\
	    -e "s,%imx-boot_decrypt_path%,flash-spl-enc-dummy.bin,g"	\
	"${SCRIPT_PATH}/csf_templates/encrypt_sign_uboot_spl" > csf_spl_sign_enc.txt

	# FIT Encryption
	sed -e "${NO_TEE_SED_FILTER}"				\
	    -e "s,%srk_table%,${SRK_TABLE},g "			\
	    -e "s,%key_index%,${CONFIG_KEY_INDEX},g"		\
	    -e "s,%cert_csf%,${CERT_CSF},g"			\
	    -e "s,%cert_img%,${CERT_IMG},g"			\
	    -e "s,%sld_auth_start%,${sld_ram_start},g"		\
	    -e "s,%sld_auth_offset%,${sld_header_offset},g"	\
	    -e "s,%sld_auth_len%,${sld_image_len},g"		\
	    -e "s,%imx-boot_auth_path%,flash-spl-enc.bin,g"	\
	    -e "s,%dek_path%,${CONFIG_DEK_PATH},g"		\
	    -e "s,%dek_len%,${dek_size},g"			\
	    -e "s,%dek_offset%,${fit_dek_blob_addr},g"		\
	    -e "s,%uboot_decrypt_start%,${uboot_dtb_ram_start},g"\
	    -e "s,%uboot_decrypt_offset%,${uboot_dtb_image_offset},g"\
	    -e "s,%uboot_decrypt_len%,${uboot_dtb_image_len},g"	\
	    -e "s,%atf_decrypt_start%,${atf_ram_start},g"	\
	    -e "s,%atf_decrypt_offset%,${atf_image_offset},g"	\
	    -e "s,%atf_decrypt_len%,${atf_image_len},g"		\
	    -e "s,%optee_decrypt_start%,${optee_ram_start},g"       \
	    -e "s,%optee_decrypt_offset%,${optee_image_offset},g"   \
	    -e "s,%optee_decrypt_len%,${optee_image_len},g"         \
	    -e "s,%imx-boot_decrypt_path%,flash-spl-fit-enc.bin,g"	\
	"${SCRIPT_PATH}/csf_templates/encrypt_uboot_fit" > csf_fit_enc.txt

	sed -e "${NO_TEE_SED_FILTER}"				\
	    -e "s,%srk_table%,${SRK_TABLE},g "			\
	    -e "s,%key_index%,${CONFIG_KEY_INDEX},g"		\
	    -e "s,%cert_csf%,${CERT_CSF},g"			\
	    -e "s,%cert_img%,${CERT_IMG},g"			\
	    -e "s,%sld_auth_start%,${sld_ram_start},g"		\
	    -e "s,%sld_auth_offset%,${sld_header_offset},g"	\
	    -e "s,%sld_auth_len%,${sld_image_len},g"		\
	    -e "s,%uboot_auth_start%,${uboot_dtb_ram_start},g"	\
	    -e "s,%uboot_auth_offset%,${uboot_dtb_image_offset},g"	\
	    -e "s,%uboot_auth_len%,${uboot_dtb_image_len},g"	\
	    -e "s,%atf_auth_start%,${atf_ram_start},g"		\
	    -e "s,%atf_auth_offset%,${atf_image_offset},g"	\
	    -e "s,%atf_auth_len%,${atf_image_len},g"		\
	    -e "s,%optee_auth_start%,${optee_ram_start},g"      \
	    -e "s,%optee_auth_offset%,${optee_image_offset},g"  \
	    -e "s,%optee_auth_len%,${optee_image_len},g"        \
	    -e "s,%imx-boot_auth_path%,flash-spl-fit-enc.bin,g"	\
	    -e "s,%dek_path%,${CONFIG_DEK_PATH},g"		\
	    -e "s,%dek_len%,${dek_size},g"			\
	    -e "s,%dek_offset%,${fit_dek_blob_addr},g"		\
	    -e "s,%uboot_decrypt_start%,${uboot_dtb_ram_start},g"\
	    -e "s,%uboot_decrypt_offset%,${uboot_dtb_image_offset},g"\
	    -e "s,%uboot_decrypt_len%,${uboot_dtb_image_len},g"	\
	    -e "s,%atf_decrypt_start%,${atf_ram_start},g"	\
	    -e "s,%atf_decrypt_offset%,${atf_image_offset},g"	\
	    -e "s,%atf_decrypt_len%,${atf_image_len},g"		\
	    -e "s,%optee_decrypt_start%,${optee_ram_start},g"   \
	    -e "s,%optee_decrypt_offset%,${optee_image_offset},g" \
	    -e "s,%optee_decrypt_len%,${optee_image_len},g"     \
	    -e "s,%imx-boot_decrypt_path%,flash-spl-fit-enc-dummy.bin,g"\
	"${SCRIPT_PATH}/csf_templates/encrypt_sign_uboot_fit" > csf_fit_sign_enc.txt
else
	sed -e "s,%srk_table%,${SRK_TABLE},g "			\
	    -e "s,%key_index%,${CONFIG_KEY_INDEX},g"		\
	    -e "s,%cert_csf%,${CERT_CSF},g"			\
	    -e "s,%cert_img%,${CERT_IMG},g"			\
	    -e "s,%spl_auth_start%,${spl_ram_start},g"		\
	    -e "s,%spl_auth_offset%,${spl_header_offset},g"	\
	    -e "s,%spl_auth_len%,${spl_image_len},g"		\
	    -e "s,%imx-boot_path%,${TARGET},g"			\
	"${SCRIPT_PATH}/csf_templates/sign_uboot_spl" > csf_spl.txt

	sed -e "${NO_TEE_SED_FILTER}"				\
	    -e "s,%srk_table%,${SRK_TABLE},g "			\
	    -e "s,%key_index%,${CONFIG_KEY_INDEX},g"		\
	    -e "s,%cert_csf%,${CERT_CSF},g"			\
	    -e "s,%cert_img%,${CERT_IMG},g"			\
	    -e "s,%sld_auth_start%,${sld_ram_start},g"		\
	    -e "s,%sld_auth_offset%,${sld_header_offset},g"	\
	    -e "s,%sld_auth_len%,${sld_image_len},g"		\
	    -e "s,%uboot_auth_start%,${uboot_ram_start},g"	\
	    -e "s,%uboot_auth_offset%,${uboot_image_offset},g"	\
	    -e "s,%uboot_auth_len%,${uboot_image_len},g"	\
	    -e "s,%dtb_auth_start%,${dtb_ram_start},g"		\
	    -e "s,%dtb_auth_offset%,${dtb_image_offset},g"	\
	    -e "s,%dtb_auth_len%,${dtb_image_len},g"		\
	    -e "s,%atf_auth_start%,${atf_ram_start},g"		\
	    -e "s,%atf_auth_offset%,${atf_image_offset},g"	\
	    -e "s,%atf_auth_len%,${atf_image_len},g"		\
	    -e "s,%optee_auth_start%,${optee_ram_start},g"      \
	    -e "s,%optee_auth_offset%,${optee_image_offset},g"  \
	    -e "s,%optee_auth_len%,${optee_image_len},g"        \
	    -e "s,%imx-boot_path%,${TARGET},g"			\
	"${SCRIPT_PATH}/csf_templates/sign_uboot_fit" > csf_fit.txt
fi

# If requested, instruct HAB not to protect the SRK_REVOKE OTP field
if [ -n "${CONFIG_UNLOCK_SRK_REVOKE}" ]; then
	SIGN_CSF="csf_spl.txt"
	[ "${ENCRYPT}" = "true" ] && SIGN_CSF="csf_spl_sign_enc.txt"
	{
		echo ""
		echo "[Unlock]"
		echo "    Engine = OCOTP"
		echo "    Features = SRK REVOKE"
	} >> ${SIGN_CSF}
fi

# Generate SRK tables
if ! srktool --hab_ver 4 --certs "${SRK_KEYS}" --table "${SRK_TABLE}" --efuses "${SRK_EFUSES}" --digest sha256; then
	echo "[ERROR] Could not generate SRK tables"
	exit 1
fi

CURRENT_PATH="$(pwd)"

if [ "${ENCRYPT}" != "true" ]; then
	# Generate signed uboot
	cp "${UBOOT_PATH}" "${TARGET}"

	if ! cst -o "${CURRENT_PATH}/csf_spl.bin" -i "${CURRENT_PATH}/csf_spl.txt" >/dev/null; then
		echo "[ERROR] Could not generate SPL CSF"
		exit 1
	fi
	if ! cst -o "${CURRENT_PATH}/csf_fit.bin" -i "${CURRENT_PATH}/csf_fit.txt" >/dev/null; then
		echo "[ERROR] Could not generate FIT CSF"
		exit 1
	fi

	dd if="${CURRENT_PATH}/csf_spl.bin" of="${TARGET}" seek=$((spl_csf_offset)) bs=1 conv=notrunc >/dev/null 2>&1
	dd if="${CURRENT_PATH}/csf_fit.bin" of="${TARGET}" seek=$((sld_csf_offset)) bs=1 conv=notrunc >/dev/null 2>&1
else
	# Generate encrypted uboot
	# Encrypt SPL
	cp "${UBOOT_PATH}" flash-spl-enc.bin
	if ! cst -o "${CURRENT_PATH}/csf_spl_enc.bin" -i "${CURRENT_PATH}/csf_spl_enc.txt" >/dev/null; then
		echo "[ERROR] Could not generate SPL ENC CSF"
		exit 1
	fi
	# Sign encrypted SPL
	cp flash-spl-enc.bin flash-spl-enc-dummy.bin
	if ! cst -o "${CURRENT_PATH}/csf_spl_sign_enc.bin" -i "${CURRENT_PATH}/csf_spl_sign_enc.txt" >/dev/null; then
		echo "[ERROR] Could not generate SPL SIGN ENC CSF"
		exit 1
	fi
	# Encrypt FIT
	cp flash-spl-enc.bin flash-spl-fit-enc.bin
	if ! cst -o "${CURRENT_PATH}/csf_fit_enc.bin" -i "${CURRENT_PATH}/csf_fit_enc.txt" >/dev/null; then
		echo "[ERROR] Could not generate FIT ENC CSF"
		exit 1
	fi
	# Sign encrypted FIT
	cp flash-spl-fit-enc.bin flash-spl-fit-enc-dummy.bin
	if ! cst -o "${CURRENT_PATH}/csf_fit_sign_enc.bin" -i "${CURRENT_PATH}/csf_fit_sign_enc.txt" >/dev/null; then
		echo "[ERROR] Could not generate FIT SIGN ENC CSF"
		exit 1
	fi

	# Create final CSF for SPL
	hab_tag_mac="AC00244"
	nonce_mac_size="36"
	csf_size="$(stat -L -c %s csf_spl_enc.bin)"
	nonce_offset="$(hexdump -ve '1/1 "%.2X"' csf_spl_enc.bin | grep -ob -e "${hab_tag_mac}" | awk -F: '{gsub(/ /, ""); print int($1/2)}')"
	echo "SPL ENC csf_size: ${csf_size} / nonce_offset: ${nonce_offset}"
	dd if=csf_spl_enc.bin of=noncemac.bin bs=1 skip=${nonce_offset} count="${nonce_mac_size}" conv=notrunc
	csf_size="$(stat -L -c %s csf_spl_sign_enc.bin)"
	nonce_offset="$(hexdump -ve '1/1 "%.2X"' csf_spl_sign_enc.bin | grep -ob -e "${hab_tag_mac}" | awk -F: '{gsub(/ /, ""); print int($1/2)}')"
	echo "SPL SIGN ENC csf_size: ${csf_size} / nonce_offset: ${nonce_offset}"
	dd if=noncemac.bin of=csf_spl_sign_enc.bin bs=1 seek=${nonce_offset} count="${nonce_mac_size}" conv=notrunc

	# Create final CSF for FIT
	csf_size="$(stat -L -c %s csf_fit_enc.bin)"
	nonce_offset="$(hexdump -ve '1/1 "%.2X"' csf_fit_enc.bin | grep -ob -e "${hab_tag_mac}" | awk -F: '{gsub(/ /, ""); print int($1/2)}')"
	echo "FIT ENC csf_size: ${csf_size} / nonce_offset: ${nonce_offset}"
	dd if=csf_fit_enc.bin of=noncemac.bin bs=1 skip=${nonce_offset} count="${nonce_mac_size}" conv=notrunc
	csf_size="$(stat -L -c %s csf_fit_sign_enc.bin)"
	nonce_offset="$(hexdump -ve '1/1 "%.2X"' csf_fit_sign_enc.bin | grep -ob -e "${hab_tag_mac}" | awk -F: '{gsub(/ /, ""); print int($1/2)}')"
	echo "FIT SIGN ENC csf_size: ${csf_size} / nonce_offset: ${nonce_offset}"
	dd if=noncemac.bin of=csf_fit_sign_enc.bin bs=1 seek=${nonce_offset} count="${nonce_mac_size}" conv=notrunc

	cp flash-spl-fit-enc.bin "${TARGET}"
	dd if="${CURRENT_PATH}/csf_spl_sign_enc.bin" of="${TARGET}" seek=$((spl_csf_offset)) bs=1 conv=notrunc >/dev/null 2>&1
	dd if="${CURRENT_PATH}/csf_fit_sign_enc.bin" of="${TARGET}" seek=$((sld_csf_offset)) bs=1 conv=notrunc >/dev/null 2>&1
fi

[ "${ENCRYPT}" = "true" ] && ENCRYPTED_MSG="and encrypted "
echo "Signed ${ENCRYPTED_MSG}image ready: ${TARGET}"

rm -f "${SRK_TABLE}" flash-spl-* csf_* 2>/dev/null
