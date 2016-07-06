#!/bin/bash
#===============================================================================
#
#  sign.sh
#
#  Copyright (C) 2016 by Digi International Inc.
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
#      CONFIG_CST_PATH: (mandatory) path to the CST folder by NXP with keys generated.
#      CONFIG_KEY_INDEX: (optional) key index to use for signing. Default is 0.
#      CONFIG_ENCRYPT_IMAGE: (optional) define it to build encrypted images.
#      CONFIG_DEK_SIZE: (optional) size of the DEK. Allowed values: 128 (default), 192, 256.
#
#===============================================================================

# Get enviroment variables from .config
. .config

# External tools are used, so UBOOT_PATH has to be absolute
UBOOT_PATH="$(readlink -e $1)"
TARGET="${2}"

# The CST uses the same file as input and output.
# We want to keep the original U-Boot image unmodified, so
# make a copy of it and restore it as the last step.
cp "${UBOOT_PATH}" "${UBOOT_PATH}-orig"

# Check arguments
if [ -z "${CONFIG_CST_PATH}" ]; then
	echo "Undefined CONFIG_CST_PATH";
	exit 1
elif [ ! -d "${CONFIG_CST_PATH}" ]; then
	echo "Invalid CONFIG_CST_PATH: ${CONFIG_CST_PATH}"
	exit 1
fi

if [ ! -z "${CONFIG_ENCRYPT_IMAGE}" ]; then
	ENCRYPT="true"
fi

# Default values
[ -z "${CONFIG_KEY_INDEX}" ] && CONFIG_KEY_INDEX="0"
CONFIG_KEY_INDEX_1="$((CONFIG_KEY_INDEX+1))"
[ -z "${CONFIG_DEK_SIZE}" ] && CONFIG_DEK_SIZE="128"

SRK_KEYS="$(echo ${CONFIG_CST_PATH}/crts/SRK*crt.pem | sed s/\ /\,/g)"
CERT_CSF="$(echo ${CONFIG_CST_PATH}/crts/CSF${CONFIG_KEY_INDEX_1}*crt.pem)"
CERT_IMG="$(echo ${CONFIG_CST_PATH}/crts/IMG${CONFIG_KEY_INDEX_1}*crt.pem)"

# Path to export the DEK used in plain text
UBOOT_DEK_PATH="$(pwd)/dek.bin"
SRK_TABLE="$(pwd)/SRK_table.bin"
SRK_EFUSES="$(pwd)/SRK_efuses.bin"

# Separator between signed and encrypted zones
# According the HAB requirements, this should *not* contain the following regions:
# IVT, DCD, Boot data, first word of the uboot image.
ENCRYPT_START="0xC10"

# Other constants
GAP_FILLER="0x00"
PADDED_UBOOT_PATH="$(pwd)/u-boot-pad.imx"
IVT_OFFSET="0"
UBOOT_START_OFFSET="0x400"

if [ "$(uname -m)" = "x86_64" ]; then
	ARCH="64"
else
	ARCH="32"
fi

# Parse uboot IVT
ivt_self=$(hexdump -n 4 -s 20 -e '/4 "0x%08x\t" "\n"' ${UBOOT_PATH})
ivt_csf=$(hexdump -n 4 -s 24 -e '/4 "0x%08x\t" "\n"' ${UBOOT_PATH})
ddr_addr=$(hexdump -n 4 -s 32 -e '/4 "0x%08x\t" "\n"' ${UBOOT_PATH})

# Compute dek blob size in bytes:
# header (8) + 256-bit AES key (32) + MAC (16) + custom key size in bytes
dek_blob_size="$((8 + 32 + 16 + CONFIG_DEK_SIZE/8))"

# It is important to abort in this case because running the rest of the logic
# with a null csf pointer will attempt to create huge paddings causing problems
# in the development machine.
if [ $((ivt_csf)) -eq 0 ]; then
	echo "Invalid CSF pointer in the IVT table (is SECURE_BOOT enabled?)"
	exit 1
fi

# Compute the layout: sizes and offsets.
uboot_size="$(stat -c %s ${UBOOT_PATH})"
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

# Generate actual CSF descriptor file from template
if [ "${ENCRYPT}" = "true" ]; then
	sed -e "s,%ram_start%,${ivt_start},g"                  \
	    -e "s,%srk_table%,${SRK_TABLE},g "                 \
	    -e "s,%image_offset%,${IVT_OFFSET},g"              \
	    -e "s,%auth_len%,${ENCRYPT_START},g"               \
	    -e "s,%cert_csf%,${CERT_CSF},g"                    \
	    -e "s,%cert_img%,${CERT_IMG},g"                    \
	    -e "s,%uboot_path%,${PADDED_UBOOT_PATH},g"         \
	    -e "s,%key_index%,${CONFIG_KEY_INDEX},g"           \
	    -e "s,%dek_len%,${CONFIG_DEK_SIZE},g"              \
	    -e "s,%dek_path%,${UBOOT_DEK_PATH},g"              \
	    -e "s,%dek_offset%,${dek_blob_offset},g"           \
	    -e "s,%ram_decrypt_start%,${ram_decrypt_start},g"  \
	    -e "s,%image_decrypt_offset%,${ENCRYPT_START},g"   \
	    -e "s,%decrypt_len%,${decrypt_len},g"              \
	${srctree}/scripts/csf_encrypt_template > csf_descriptor
else
	sed -e "s,%ram_start%,${ivt_start},g"          \
	    -e "s,%srk_table%,${SRK_TABLE},g"          \
	    -e "s,%image_offset%,${IVT_OFFSET},g"      \
	    -e "s,%auth_len%,${auth_len},g"            \
	    -e "s,%cert_csf%,${CERT_CSF},g"            \
	    -e "s,%cert_img%,${CERT_IMG},g"            \
	    -e "s,%uboot_path%,${PADDED_UBOOT_PATH},g" \
	    -e "s,%key_index%,${CONFIG_KEY_INDEX},g"   \
	${srctree}/scripts/csf_sign_template > csf_descriptor
fi

# Generate SRK tables
if pushd "${CONFIG_CST_PATH}/linux${ARCH}" > /dev/null; then
	./srktool --hab_ver 4 --certs "${SRK_KEYS}" --table "${SRK_TABLE}" --efuses "${SRK_EFUSES}" --digest sha256
	if [ $? -ne 0 ]; then
		echo "[ERROR] Could not generate SRK tables"
		exit 1
	fi
	popd > /dev/null
else
	echo "Bad path: ${CONFIG_CST_PATH}/linux${ARCH}"
	exit 1
fi

# Generate signed uboot (add padding, generate signature and ensamble final image)
objcopy -I binary -O binary --pad-to "${pad_len}" --gap-fill="${GAP_FILLER}" "${UBOOT_PATH}" u-boot-pad.imx

CURRENT_PATH="$(pwd)"
if pushd "${CONFIG_CST_PATH}/linux${ARCH}" > /dev/null; then
	./cst -o "${CURRENT_PATH}/u-boot_csf.bin" -i "${CURRENT_PATH}/csf_descriptor"
	if [ $? -ne 0 ]; then
		echo "[ERROR] Could not generate CSF"
		exit 1
	fi
	popd > /dev/null
else
	echo "Bad path: ${CONFIG_CST_PATH}/linux${ARCH}"
	exit 1
fi

cat u-boot-pad.imx u-boot_csf.bin > u-boot-signed-no-pad.imx

# When using encrypted uboot, we need to leave room for the DEK blob (will be appended manually)
if [ "${ENCRYPT}" = "true" ]; then
	sig_len="$((sig_len - dek_blob_size))"
fi

objcopy -I binary -O binary --pad-to "${sig_len}" --gap-fill="${GAP_FILLER}"  u-boot-signed-no-pad.imx "${TARGET}"

# Restore the original U-Boot image to undo any transformations that have been made during
# the signature/encryption process.
mv "${UBOOT_PATH}-orig" "${UBOOT_PATH}"

# When CONFIG_CSF_SIZE is defined, the CSF pointer on the
# IVT is set to the CSF future location. The final image has the CSF block
# appended, so this is consistent.
# On the other hand, the u-boot.imx artifact is in a inconsistent state: the CSF
# pointer is not zero, but it does not have a CSF appended.
# This can cause problems because the HAB and CAAM will execute
# instructions from an uninitialized region of memory.

# Erase the CSF pointer of the unsigned artifact to avoid that problem.
# Note: this pointer is set during compilation, not in this script.
printf '\x0\x0\x0\x0' | dd conv=notrunc of=${UBOOT_PATH} bs=4 seek=6

rm -f "${SRK_TABLE}" csf_descriptor u-boot_csf.bin u-boot-pad.imx u-boot-signed-no-pad.imx u-boot-encrypted.imx 2> /dev/null
