#!/bin/bash
#===============================================================================
#
#  sign.sh
#
#  Copyright (C) 2016-2021 by Digi International Inc.
#  All rights reserved.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License version 2 as published by
#  the Free Software Foundation.
#
#
#  Description:
#    Script for building signed and encrypted uboot images using NXP CST.
#
#    The following Kconfig entries are used:
#      CONFIG_SIGN_KEYS_PATH: (mandatory) path to the CST folder by NXP with keys generated.
#      CONFIG_KEY_INDEX: (optional) key index to use for signing. Default is 0.
#      CONFIG_UNLOCK_SRK_REVOKE: (optional) instruct HAB not to protect the SRK_REVOKE OTP
#				  field so that key revocation is possible in closed devices.
#      SRK_REVOKE_MASK: (optional, AHAB only) set bitmask of the revoked SRKs.
#      ENABLE_USB_SIGN: (optional) if defined, signed images could be used to boot via USB.
#      ENABLE_ENCRYPTION: (optional) enable encryption of the images.
#      CONFIG_DEK_PATH: (mandatory if ENCRYPT is defined) path to a Data Encryption Key.
#                       If defined, the signed	U-Boot image is encrypted with the
#                       given key. Supported key sizes: 128, 192 and 256 bits.
#      CONFIG_MKIMAGE_LOG_PATH: (optional, AHAB only) path to the log generated
#                               when imx-boot was compiled. If not provided, a default
#                               'mkimage.log' file is expected in current directory.
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
	echo "Usage: ${SCRIPT_NAME} <input-unsigned-uboot> <output-signed-uboot>"
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
if [ -z "${CONFIG_SIGN_MODE}" ]; then
	echo "Undefined CONFIG_SIGN_MODE";
	exit 1
fi

# Get DEK key
if [ -n "${CONFIG_DEK_PATH}" ] && [ -n "${ENABLE_ENCRYPTION}" ]; then
	if [ ! -f "${CONFIG_DEK_PATH}" ]; then
		echo "DEK not found. Generating random 256 bit DEK."
		[ -d $(dirname ${CONFIG_DEK_PATH}) ] || mkdir -p $(dirname ${CONFIG_DEK_PATH})
		if ! dd if=/dev/urandom of="${CONFIG_DEK_PATH}" bs=32 count=1 >/dev/null; then
			echo "[ERROR] Could not generate random DEK"
			exit 1
		fi
	fi
	dek_size="$((8 * $(stat -L -c %s ${CONFIG_DEK_PATH})))"
	if [ "${dek_size}" != "128" ] && [ "${dek_size}" != "192" ] && [ "${dek_size}" != "256" ]; then
		echo "Invalid DEK size: ${dek_size} bits. Valid sizes are 128, 192 and 256 bits"
		exit 1
	fi
	ENCRYPT="true"
fi

if [ "${CONFIG_SIGN_MODE}" = "HAB" ]; then
	HAB_VER="hab_ver 4"
	DIGEST="digest"
	DIGEST_ALGO="sha256"
	if [ -n "${ENABLE_USB_SIGN}" ]; then
		SIGN_DCD="true"
	fi
elif [ "${CONFIG_SIGN_MODE}" = "AHAB" ]; then
	HAB_VER="ahab"
	DIGEST="sign_digest"
	DIGEST_ALGO="sha512"
else
	echo "Invalid CONFIG_SIGN_MODE."
	exit 1
fi

# Default values
[ -z "${CONFIG_KEY_INDEX}" ] && CONFIG_KEY_INDEX="0"
CONFIG_KEY_INDEX_1="$((CONFIG_KEY_INDEX + 1))"

SRK_KEYS="$(echo ${CONFIG_SIGN_KEYS_PATH}/crts/SRK*crt.pem | sed s/\ /\,/g)"
n_commas="$(echo ${SRK_KEYS} | grep -o "," | wc -l)"

if [ "${CONFIG_SIGN_MODE}" = "HAB" ]; then
	[ -z "${CONFIG_CSF_SIZE}" ] && CONFIG_CSF_SIZE="0x4000"
	CERT_CSF="$(echo ${CONFIG_SIGN_KEYS_PATH}/crts/CSF${CONFIG_KEY_INDEX_1}*crt.pem)"
	CERT_IMG="$(echo ${CONFIG_SIGN_KEYS_PATH}/crts/IMG${CONFIG_KEY_INDEX_1}*crt.pem)"

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
else
	CERT_SRK="$(echo ${CONFIG_SIGN_KEYS_PATH}/crts/SRK${CONFIG_KEY_INDEX_1}*crt.pem | sed s/\ /\,/g)"

	if [ "${n_commas}" -eq 3 ] && [ -f "${CERT_SRK}" ]; then
		# PKI tree already exists. Do nothing
		echo "Using existing PKI tree"
	elif [ "${n_commas}" -eq 0 ] || [ ! -f "${CERT_SRK}" ]; then
		# Generate PKI
		trustfence-gen-pki.sh "${CONFIG_SIGN_KEYS_PATH}"

		SRK_KEYS="$(echo ${CONFIG_SIGN_KEYS_PATH}/crts/SRK*crt.pem | sed s/\ /\,/g)"
		CERT_SRK="$(echo ${CONFIG_SIGN_KEYS_PATH}/crts/SRK${CONFIG_KEY_INDEX_1}*crt.pem | sed s/\ /\,/g)"
	else
		echo "Inconsistent CST folder."
		exit 1
	fi
fi

# Path for the SRK table and eFuses file
SRK_TABLE="$(pwd)/SRK_table.bin"
SRK_EFUSES="$(pwd)/SRK_efuses.bin"

# Parse data and generate CSF descriptor file
if [ "${CONFIG_SIGN_MODE}" = "HAB" ]; then
	# Separator between signed and encrypted zones
	# According the HAB requirements, this should *not* contain the following regions:
	# IVT, DCD, Boot data, first word of the uboot image.
	ENCRYPT_START="0xC10"

	# Other constants
	GAP_FILLER="0x00"
	PADDED_UBOOT_PATH="$(pwd)/u-boot-pad.imx"
	IVT_OFFSET="0"
	UBOOT_START_OFFSET="0x400"
	CONFIG_IMX_DCD_ADDR="0x00910000"
	DCD_OFFSET="0x2C"

	# Parse uboot IVT and size
	ivt_self=$(hexdump -n 4 -s 20 -e '/4 "0x%08x\t" "\n"' ${UBOOT_PATH})
	ivt_csf=$(hexdump -n 4 -s 24 -e '/4 "0x%08x\t" "\n"' ${UBOOT_PATH})
	ddr_addr=$(hexdump -n 4 -s 32 -e '/4 "0x%08x\t" "\n"' ${UBOOT_PATH})
	uboot_size="$(stat -c %s ${UBOOT_PATH})"

	# If the U-Boot Image Vector Table contains a nulled CSF pointer, assume that
	# the CSF will be appended at the end. This is the case when using the script
	# outside the compilation process, to sign existing U-Boot images.
	if [ $((ivt_csf)) -eq 0 ]; then
		ivt_csf="$((uboot_size + ddr_addr + UBOOT_START_OFFSET))"
		printf $(printf "%08x" ${ivt_csf} | sed 's/.\{2\}/&\n/g' | tac | sed 's,^,\\x,g' | tr -d '\n') | dd conv=notrunc of=${UBOOT_PATH} bs=4 seek=6 >/dev/null
		if [ $? -ne 0 ]; then
			echo "[ERROR] Could not write CSF"
			exit 1
		fi
		# It is also necessary to adjust the size of the image to take into account
		# the overhead of the CSF block.
		image_size=$(hexdump -n 4 -s 36 -e '/4 "0x%08x\t" "\n"' ${UBOOT_PATH})
		image_size=$((image_size + CONFIG_CSF_SIZE))
		printf $(printf "%08x" ${image_size} | sed 's/.\{2\}/&\n/g' | tac | sed 's,^,\\x,g' | tr -d '\n') | dd conv=notrunc of=${UBOOT_PATH} bs=4 seek=9 >/dev/null
		if [ $? -ne 0 ]; then
			echo "[ERROR] Could not adjust image size"
			exit 1
		fi
	fi

	# Compute dek blob size in bytes:
	# header (8) + 256-bit AES key (32) + MAC (16) + custom key size in bytes
	dek_blob_size="$((8 + 32 + 16 + dek_size/8))"

	# Parse uboot DCD size
	dcd_len_h=$(hexdump -n 1 -s 45 -e '"%02x"' ${UBOOT_PATH})
	dcd_len_l=$(hexdump -n 1 -s 46 -e '"%02x"' ${UBOOT_PATH})
	dcd_len="0x${dcd_len_h}${dcd_len_l}"

	# Compute the layout: sizes and offsets.
	pad_len="$((ivt_csf - ivt_self))"
	sig_len="$((pad_len + CONFIG_CSF_SIZE))"
	auth_len="$((pad_len - IVT_OFFSET))"
	ivt_start="$((ddr_addr + UBOOT_START_OFFSET))"
	ram_decrypt_start="$((ivt_start + ENCRYPT_START))"
	decrypt_len="$((pad_len - ENCRYPT_START))"
	dek_blob_offset="$((ivt_start + auth_len + CONFIG_CSF_SIZE - dek_blob_size))"

	# Change values to hex strings
	pad_len="$(printf "0x%X" ${pad_len})"
	sig_len="$(printf "0x%X" ${sig_len})"
	auth_len="$(printf "0x%X" ${auth_len})"
	ivt_start="$(printf "0x%X" ${ivt_start})"
	ram_decrypt_start="$(printf "0x%X" ${ram_decrypt_start})"
	decrypt_len="$(printf "0x%X" ${decrypt_len})"
	dek_blob_offset="$(printf "0x%X" ${dek_blob_offset})"
	dcd_len="$(printf "0x%X" ${dcd_len})"

	# Generate actual CSF descriptor file from template
	if [ "${ENCRYPT}" = "true" ]; then
		# Sign and encrypt the U-Boot memory block
		sed -e "s,%ram_start%,${ivt_start},g"		       \
		-e "s,%srk_table%,${SRK_TABLE},g "		       \
		-e "s,%image_offset%,${IVT_OFFSET},g"	       \
		-e "s,%auth_len%,${ENCRYPT_START},g"	       \
		-e "s,%cert_csf%,${CERT_CSF},g"		       \
		-e "s,%cert_img%,${CERT_IMG},g"		       \
		-e "s,%uboot_path%,${PADDED_UBOOT_PATH},g"	       \
		-e "s,%key_index%,${CONFIG_KEY_INDEX},g"	       \
		-e "s,%dek_len%,${dek_size},g"		       \
		-e "s,%dek_path%,${CONFIG_DEK_PATH},g"	       \
		-e "s,%dek_offset%,${dek_blob_offset},g"	       \
		-e "s,%ram_decrypt_start%,${ram_decrypt_start},g"  \
		-e "s,%image_decrypt_offset%,${ENCRYPT_START},g"   \
		-e "s,%decrypt_len%,${decrypt_len},g"	       \
		${SCRIPT_PATH}/csf_templates/encrypt_uboot > csf_descriptor
	elif [ "${SIGN_DCD}" = "true" ]; then
		# Sign both U-Boot and DCD memory blocks
		sed -e "s,%ram_start%,${ivt_start},g"		\
		-e "s,%srk_table%,${SRK_TABLE},g"		\
		-e "s,%image_offset%,${IVT_OFFSET},g"		\
		-e "s,%auth_len%,${auth_len},g"			\
		-e "s,%dcd_start%,${CONFIG_IMX_DCD_ADDR},g"	\
		-e "s,%dcd_offset%,${DCD_OFFSET},g"		\
		-e "s,%dcd_len%,${dcd_len},g"			\
		-e "s,%cert_csf%,${CERT_CSF},g"			\
		-e "s,%cert_img%,${CERT_IMG},g"			\
		-e "s,%uboot_path%,${PADDED_UBOOT_PATH},g"	\
		-e "s,%key_index%,${CONFIG_KEY_INDEX},g"	\
		${SCRIPT_PATH}/csf_templates/sign_uboot_dcd > csf_descriptor
	else
		# Sign the U-Boot memory block
		sed -e "s,%ram_start%,${ivt_start},g"	       \
		-e "s,%srk_table%,${SRK_TABLE},g"	       \
		-e "s,%image_offset%,${IVT_OFFSET},g"      \
		-e "s,%auth_len%,${auth_len},g"	       \
		-e "s,%cert_csf%,${CERT_CSF},g"	       \
		-e "s,%cert_img%,${CERT_IMG},g"	       \
		-e "s,%uboot_path%,${PADDED_UBOOT_PATH},g" \
		-e "s,%key_index%,${CONFIG_KEY_INDEX},g"   \
		${SCRIPT_PATH}/csf_templates/sign_uboot > csf_descriptor
	fi

	# If requested, instruct HAB not to protect the SRK_REVOKE OTP field
	if [ -n "${CONFIG_UNLOCK_SRK_REVOKE}" ]; then
		echo "" >> csf_descriptor
		echo "[Unlock]" >> csf_descriptor
		echo "    Engine = OCOTP" >> csf_descriptor
		echo "    Features = SRK REVOKE" >> csf_descriptor
	fi
else
	[ -z "${SRK_REVOKE_MASK}" ] && SRK_REVOKE_MASK="0x0"
	if [ "$((SRK_REVOKE_MASK & 0x8))" != 0 ]; then
		echo "Key 3 cannot be revoked. Removed from mask."
		SRK_REVOKE_MASK=$((SRK_REVOKE_MASK - 8))
	fi

	# Path to log file to parse
	MKIMAGE_LOG="$(pwd)/mkimage.log"
	[ -n "${CONFIG_MKIMAGE_LOG_PATH}" ] && MKIMAGE_LOG="${CONFIG_MKIMAGE_LOG_PATH}"

	if [ ! -e "${MKIMAGE_LOG}" ]; then
		echo "Make log '${MKIMAGE_LOG}' does not exist."
		exit 1
	fi
	echo "Using make log '${MKIMAGE_LOG}'"

	# Parse Container and Signature block offsets
	container_header_offset=$(awk '/CST: CONTAINER 0 offset:/ {print $5}' ${MKIMAGE_LOG})
	signature_block_offset=$(awk '/CST: CONTAINER 0: Signature Block:/ {print $9}' ${MKIMAGE_LOG})

	# Generate actual CSF descriptor file from template
	if [ "${ENCRYPT}" = "true" ]; then
		sed -e "s,%srk_table%,${SRK_TABLE},g" \
		-e "s,%cert_img%,${CERT_SRK},g" \
		-e "s,%key_index%,${CONFIG_KEY_INDEX},g" \
		-e "s,%srk_rvk_mask%,${SRK_REVOKE_MASK},g" \
		-e "s,%u-boot-img%,${UBOOT_PATH},g"   \
		-e "s,%container_offset%,${container_header_offset},g" \
		-e "s,%block_offset%,${signature_block_offset},g" \
		-e "s,%dek_len%,${dek_size},g" \
		-e "s,%dek_path%,${CONFIG_DEK_PATH},g" \
		${SCRIPT_PATH}/csf_templates/encrypt_ahab_uboot > csf_descriptor
	else
		sed -e "s,%srk_table%,${SRK_TABLE},g" \
		-e "s,%cert_img%,${CERT_SRK},g" \
		-e "s,%key_index%,${CONFIG_KEY_INDEX},g" \
		-e "s,%srk_rvk_mask%,${SRK_REVOKE_MASK},g" \
		-e "s,%u-boot-img%,${UBOOT_PATH},g"   \
		-e "s,%container_offset%,${container_header_offset},g" \
		-e "s,%block_offset%,${signature_block_offset},g" \
		${SCRIPT_PATH}/csf_templates/sign_ahab_uboot > csf_descriptor
	fi
fi

# Generate SRK tables
srktool --${HAB_VER} --certs "${SRK_KEYS}" --table "${SRK_TABLE}" --efuses "${SRK_EFUSES}" --${DIGEST} "${DIGEST_ALGO}"
if [ $? -ne 0 ]; then
	echo "[ERROR] Could not generate SRK tables"
	exit 1
fi

# Sign/encrypt and add padding to the resulting file
if [ "${CONFIG_SIGN_MODE}" = "HAB" ]; then
	# Generate signed uboot (add padding, generate signature and ensamble final image)
	objcopy -I binary -O binary --pad-to "${pad_len}" --gap-fill="${GAP_FILLER}" "${UBOOT_PATH}" "${PADDED_UBOOT_PATH}"

	CURRENT_PATH="$(pwd)"
	cst -o "${CURRENT_PATH}/u-boot_csf.bin" -i "${CURRENT_PATH}/csf_descriptor" > /dev/null
	if [ $? -ne 0 ]; then
		echo "[ERROR] Could not generate CSF"
		exit 1
	fi

	cat "${PADDED_UBOOT_PATH}" u-boot_csf.bin > u-boot-signed-no-pad.imx

	# When using encrypted uboot, we need to leave room for the DEK blob (will be appended manually)
	if [ "${ENCRYPT}" = "true" ]; then
		sig_len="$((sig_len - dek_blob_size))"
	fi

	objcopy -I binary -O binary --pad-to "${sig_len}" --gap-fill="${GAP_FILLER}"  u-boot-signed-no-pad.imx "${TARGET}"

	# When CONFIG_CSF_SIZE is defined, the CSF pointer on the
	# IVT is set to the CSF future location. The final image has the CSF block
	# appended, so this is consistent.
	# On the other hand, the u-boot.imx artifact is in a inconsistent state: the CSF
	# pointer is not zero, but it does not have a CSF appended.
	# This can cause problems because the HAB and CAAM will execute
	# instructions from an uninitialized region of memory.

	# Erase the CSF pointer of the unsigned artifact to avoid that problem.
	# Note: this pointer is set during compilation, not in this script.
	printf '\x0\x0\x0\x0' | dd conv=notrunc of=${UBOOT_PATH} bs=4 seek=6 >/dev/null
	if [ $? -ne 0 ]; then
		echo "[ERROR] Could not erase CSF"
		exit 1
	fi

	# The $UBOOT_PATH artifact is not signed, so the size of the image
	# needs to be adjusted substracting the CSF_SIZE
	image_size=$(hexdump -n 4 -s 36 -e '/4 "0x%08x\t" "\n"' ${UBOOT_PATH})
	image_size=$((image_size - CONFIG_CSF_SIZE))
	printf $(printf "%08x" ${image_size} | sed 's/.\{2\}/&\n/g' | tac | sed 's,^,\\x,g' | tr -d '\n') | dd conv=notrunc of=${UBOOT_PATH} bs=4 seek=9 >/dev/null
	if [ $? -ne 0 ]; then
		echo "[ERROR] Could not adjust image size"
		exit 1
	fi

else
	CURRENT_PATH="$(pwd)"
	cst -o "${TARGET}" -i "${CURRENT_PATH}/csf_descriptor" > /dev/null
	if [ $? -ne 0 ]; then
		echo "[ERROR] Could not generate CSF"
		exit 1
	fi
fi

[ "${ENCRYPT}" = "true" ] && ENCRYPTED_MSG="and encrypted "
echo "Signed ${ENCRYPTED_MSG}image ready: ${TARGET}"
rm -f "${SRK_TABLE}" csf_descriptor u-boot_csf.bin "${PADDED_UBOOT_PATH}" u-boot-signed-no-pad.imx 2> /dev/null
