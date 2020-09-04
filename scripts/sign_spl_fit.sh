#!/bin/bash
#===============================================================================
#
#  sign_spl_fit.sh
#
#  Copyright (C) 2020 by Digi International Inc.
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
#
#===============================================================================

# Avoid parallel execution of this script
SINGLE_PROCESS_LOCK="/tmp/sign_script.lock.d"
trap 'rm -rf "${SINGLE_PROCESS_LOCK}"' INT TERM EXIT
while ! mkdir "${SINGLE_PROCESS_LOCK}" > /dev/null 2>&1; do
	sleep 1
done

SCRIPT_NAME="$(basename ${0})"
SCRIPT_PATH="$(cd $(dirname ${0}) && pwd)"

if [ "${#}" != "2" ]; then
	echo "Usage: ${SCRIPT_NAME} input-unsigned-imx-boot.bin output-signed-imx-boot.bin"
	exit 1
fi

# Get enviroment variables from .config
[ -f .config ] && . .config

# External tools are used, so UBOOT_PATH has to be absolute
UBOOT_PATH="$(readlink -e $1)"
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

SRK_KEYS="$(echo ${CONFIG_SIGN_KEYS_PATH}/crts/SRK*crt.pem | sed s/\ /\,/g)"
CERT_CSF="$(echo ${CONFIG_SIGN_KEYS_PATH}/crts/CSF${CONFIG_KEY_INDEX_1}*crt.pem)"
CERT_IMG="$(echo ${CONFIG_SIGN_KEYS_PATH}/crts/IMG${CONFIG_KEY_INDEX_1}*crt.pem)"

n_commas="$(echo ${SRK_KEYS} | grep -o "," | wc -l)"

if [ "${n_commas}" -eq 3 ] && [ -f "${CERT_CSF}" ] && [ -f "${CERT_IMG}" ]; then
	# PKI tree already exists. Do nothing
	echo "Using existing PKI tree"
elif [ "${n_commas}" -eq 0 ] || [ ! -f "${CERT_CSF}" ] || [ ! -f "${CERT_IMG}" ]; then
	# Generate PKI
	trustfence-gen-pki.sh "${CONFIG_SIGN_KEYS_PATH}"

	SRK_KEYS="$(echo ${CONFIG_SIGN_KEYS_PATH}/crts/SRK*crt.pem | sed s/\ /\,/g)"
	CERT_CSF="$(echo ${CONFIG_SIGN_KEYS_PATH}/crts/CSF${CONFIG_KEY_INDEX_1}*crt.pem)"
	CERT_IMG="$(echo ${CONFIG_SIGN_KEYS_PATH}/crts/IMG${CONFIG_KEY_INDEX_1}*crt.pem)"
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

if [ ! -e ${MKIMAGE_LOG} ]; then
	echo "${MKIMAGE_LOG} does not exist."
	exit 1
fi
if [ ! -e ${MKIMAGE_FIT_HAB_LOG} ]; then
	echo "${MKIMAGE_FIT_HAB_LOG} does not exist."
	exit 1
fi

# Parse spl uboot HAB block and CSF offset
spl_image_offset=$(awk '/ image_off/ {print $2}' ${MKIMAGE_LOG})
spl_csf_offset=$(awk '/ csf_off/ {print $2}' ${MKIMAGE_LOG})
spl_ram_start=$(awk '/ spl hab block:/ {print $4}' ${MKIMAGE_LOG})
spl_header_offset=$(awk '/ spl hab block:/ {print $5}' ${MKIMAGE_LOG})
spl_image_len=$(awk '/ spl hab block:/ {print $6}' ${MKIMAGE_LOG})

# Parse sld (Second Loader image) uboot HAB blocks and CSF offset
sld_csf_offset=$(awk '/ sld_csf_off/ {print $2}' ${MKIMAGE_LOG})
sld_ram_start=$(awk '/ sld hab block:/ {print $4}' ${MKIMAGE_LOG})
sld_header_offset=$(awk '/ sld hab block:/ {print $5}' ${MKIMAGE_LOG})
sld_image_len=$(awk '/ sld hab block:/ {print $6}' ${MKIMAGE_LOG})

# Parse fit uboot HAB blocks
result_row=$(awk '/print_fit_hab/ {print NR+1}' ${MKIMAGE_FIT_HAB_LOG})
uboot_ram_start=$(awk -v first_row=${result_row} 'NR==first_row+0 {print $1}' ${MKIMAGE_FIT_HAB_LOG})
uboot_image_offset=$(awk -v first_row=${result_row} 'NR==first_row+0 {print $2}' ${MKIMAGE_FIT_HAB_LOG})
uboot_image_len=$(awk -v first_row=${result_row} 'NR==first_row+0 {print $3}' ${MKIMAGE_FIT_HAB_LOG})
dtb_ram_start=$(awk -v first_row=${result_row} 'NR==first_row+1 {print $1}' ${MKIMAGE_FIT_HAB_LOG})
dtb_image_offset=$(awk -v first_row=${result_row} 'NR==first_row+1 {print $2}' ${MKIMAGE_FIT_HAB_LOG})
dtb_image_len=$(awk -v first_row=${result_row} 'NR==first_row+1 {print $3}' ${MKIMAGE_FIT_HAB_LOG})
atf_ram_start=$(awk -v first_row=${result_row} 'NR==first_row+2 {print $1}' ${MKIMAGE_FIT_HAB_LOG})
atf_image_offset=$(awk -v first_row=${result_row} 'NR==first_row+2 {print $2}' ${MKIMAGE_FIT_HAB_LOG})
atf_image_len=$(awk -v first_row=${result_row} 'NR==first_row+2 {print $3}' ${MKIMAGE_FIT_HAB_LOG})

# Generate actual CSF descriptor files from templates
sed -e "s,%srk_table%,${SRK_TABLE},g "			\
    -e "s,%key_index%,${CONFIG_KEY_INDEX},g"		\
    -e "s,%cert_csf%,${CERT_CSF},g"			\
    -e "s,%cert_img%,${CERT_IMG},g"			\
    -e "s,%spl_auth_start%,${spl_ram_start},g"		\
    -e "s,%spl_auth_offset%,${spl_header_offset},g"	\
    -e "s,%spl_auth_len%,${spl_image_len},g"		\
    -e "s,%imx-boot_path%,${TARGET},g"			\
${SCRIPT_PATH}/csf_templates/sign_uboot_spl > csf_spl.txt

sed -e "s,%srk_table%,${SRK_TABLE},g "			\
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
    -e "s,%imx-boot_path%,${TARGET},g"			\
${SCRIPT_PATH}/csf_templates/sign_uboot_fit > csf_fit.txt

# If requested, instruct HAB not to protect the SRK_REVOKE OTP field
if [ -n "${CONFIG_UNLOCK_SRK_REVOKE}" ]; then
	echo "" >> csf_spl.txt
	echo "[Unlock]" >> csf_spl.txt
	echo "    Engine = OCOTP" >> csf_spl.txt
	echo "    Features = SRK REVOKE" >> csf_spl.txt
fi

# Generate SRK tables
srktool --hab_ver 4 --certs "${SRK_KEYS}" --table "${SRK_TABLE}" --efuses "${SRK_EFUSES}" --digest sha256
if [ $? -ne 0 ]; then
	echo "[ERROR] Could not generate SRK tables"
	exit 1
fi

# Generate signed uboot
cp ${UBOOT_PATH} ${TARGET}

CURRENT_PATH="$(pwd)"
cst -o "${CURRENT_PATH}/csf_spl.bin" -i "${CURRENT_PATH}/csf_spl.txt" > /dev/null
if [ $? -ne 0 ]; then
	echo "[ERROR] Could not generate SPL CSF"
	exit 1
fi
cst -o "${CURRENT_PATH}/csf_fit.bin" -i "${CURRENT_PATH}/csf_fit.txt" > /dev/null
if [ $? -ne 0 ]; then
	echo "[ERROR] Could not generate FIT CSF"
	exit 1
fi

dd if=${CURRENT_PATH}/csf_spl.bin of=${TARGET} seek=$((${spl_csf_offset})) bs=1 conv=notrunc > /dev/null 2>&1
dd if=${CURRENT_PATH}/csf_fit.bin of=${TARGET} seek=$((${sld_csf_offset})) bs=1 conv=notrunc > /dev/null 2>&1

echo "Signed image ready: ${TARGET}"

rm -f "${SRK_TABLE}" csf_spl.txt csf_fit.txt csf_spl.bin csf_fit.bin 2> /dev/null
