#!/bin/sh
#
# Copyright 2021-2023 Digi International Inc.
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, you can obtain one at http://mozilla.org/MPL/2.0/.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# Description:
#   Sign U-Boot with SPL based on containers (AHAB, no FIT)
#
#   Following variables are used (from environment or '.config' file)
#      CONFIG_SIGN_KEYS_PATH: [mandatory] path for the AHAB PKI tree.
#      CONFIG_KEY_INDEX: [optional] key index to use. Default is 0.
#      SRK_REVOKE_MASK: [optional], bitmask of the revoked SRKs.
#      ENABLE_ENCRYPTION: (optional) enable encryption of the images.
#      CONFIG_DEK_PATH: (mandatory if ENCRYPT is defined) path to a Data Encryption Key.
#                       If defined, the signed U-Boot image is encrypted with the
#                       given key. Supported key sizes: 128 bits.
#      CONFIG_MKIMAGE_LOG_PATH: [optional] path to the mkimage log file.
#

# Avoid parallel execution of this script
SINGLE_PROCESS_LOCK="/tmp/sign_script.lock.d"
cleanup_and_exit() {
	rm -f "${CSF_UBOOT_ATF}" "${CSF_UBOOT_SPL}" "${SRK_TABLE}" "${TARGET_TMP}"
	rm -rf "${SINGLE_PROCESS_LOCK}"
}
trap 'cleanup_and_exit' INT TERM EXIT
while ! mkdir "${SINGLE_PROCESS_LOCK}" > /dev/null 2>&1; do
	sleep 1
done

SCRIPT_NAME="$(basename "${0}")"
SCRIPT_PATH="$(cd "$(dirname "${0}")" && pwd)"
MKIMAGE_LOG="$(pwd)/mkimage.log"
[ -n "${CONFIG_MKIMAGE_LOG_PATH}" ] && MKIMAGE_LOG="${CONFIG_MKIMAGE_LOG_PATH}"

to_hex() {
	printf '0x%x' "${1}"
}

is_mx9() {
	grep -i -qs 'platform.*mx9' "${MKIMAGE_LOG}"
}

# Get environment variables from .config
[ -f .config ] && . ./.config

#
# Sanity checks
#
if [ "${#}" != "2" ]; then
	echo "Usage: ${SCRIPT_NAME} <input-unsigned-uboot> <output-signed-uboot>"
	exit 1
elif [ ! -e "${MKIMAGE_LOG}" ]; then
	echo "Make log '${MKIMAGE_LOG}' does not exist."
	exit 1
elif [ -z "${CONFIG_SIGN_KEYS_PATH}" ]; then
	echo "Undefined CONFIG_SIGN_KEYS_PATH";
	exit 1
fi

# Initialize signing variables
[ -d "${CONFIG_SIGN_KEYS_PATH}" ] || mkdir -p "${CONFIG_SIGN_KEYS_PATH}"

[ -z "${CONFIG_KEY_INDEX}" ] && CONFIG_KEY_INDEX="0"
CONFIG_KEY_INDEX_1="$((CONFIG_KEY_INDEX + 1))"

# Get DEK key
if [ -n "${CONFIG_DEK_PATH}" ] && [ -n "${ENABLE_ENCRYPTION}" ]; then
	if [ ! -f "${CONFIG_DEK_PATH}" ]; then
		echo "DEK not found. Generating random 256 bit DEK."
		[ -d $(dirname ${CONFIG_DEK_PATH}) ] || mkdir -p $(dirname ${CONFIG_DEK_PATH})
		dd if=/dev/urandom of="${CONFIG_DEK_PATH}" bs=32 count=1 >/dev/null 2>&1
	fi
	dek_size="$((8 * $(stat -L -c %s ${CONFIG_DEK_PATH})))"
	if [ "${dek_size}" != "128" ] && [ "${dek_size}" != "192" ] && [ "${dek_size}" != "256" ]; then
		echo "Invalid DEK size: ${dek_size} bits. Valid sizes are 128, 192 and 256 bits"
		exit 1
	fi
	ENCRYPT="true"
fi

[ -z "${SRK_REVOKE_MASK}" ] && SRK_REVOKE_MASK="0x0"
if [ "$((SRK_REVOKE_MASK & 0x8))" != 0 ]; then
	echo "Key 3 cannot be revoked. Removed from mask."
	SRK_REVOKE_MASK="$((SRK_REVOKE_MASK - 8))"
fi

# Validate (or create) the PKI tree
SRK_KEYS="$(echo "${CONFIG_SIGN_KEYS_PATH}"/crts/SRK*crt.pem | sed s/\ /\,/g)"
CERT_SRK="$(echo "${CONFIG_SIGN_KEYS_PATH}"/crts/SRK${CONFIG_KEY_INDEX_1}*crt.pem | sed s/\ /\,/g)"
n_commas="$(echo "${SRK_KEYS}" | grep -o "," | wc -l)"
if [ "${n_commas}" -eq 3 ] && [ -f "${CERT_SRK}" ]; then
	# PKI tree already exists. Do nothing
	echo "Using existing PKI tree"
elif [ "${n_commas}" -eq 0 ] || [ ! -f "${CERT_SRK}" ]; then
	# Generate PKI
	trustfence-gen-pki.sh "${CONFIG_SIGN_KEYS_PATH}"
	SRK_KEYS="$(echo "${CONFIG_SIGN_KEYS_PATH}"/crts/SRK*crt.pem | sed s/\ /\,/g)"
	CERT_SRK="$(echo "${CONFIG_SIGN_KEYS_PATH}"/crts/SRK${CONFIG_KEY_INDEX_1}*crt.pem | sed s/\ /\,/g)"
else
	echo "Inconsistent CST folder."
	exit 1
fi

# Generate SRK tables
SRK_EFUSES="$(pwd)/SRK_efuses.bin"
SRK_TABLE="$(pwd)/SRK_table.bin"
# CCIMX8X parameters by default but updated if MX9 detected
SRKTOOL_PARAMS="-a -d sha512 -s sha512"
is_mx9 && SRKTOOL_PARAMS="-a -d sha256 -s sha512"
if ! srktool ${SRKTOOL_PARAMS} -c "${SRK_KEYS}" -t "${SRK_TABLE}" -e "${SRK_EFUSES}"; then
	echo "[ERROR] Could not generate SRK tables"
	exit 1
fi

# Parse mkimage log file to get the offsets for different containers
echo "Using make log '${MKIMAGE_LOG}'"
ATF_CONTAINER_OFFSET="$(sed -ne 's,^append[^[:digit:]]\+\([[:digit:]]\+\)[^[:digit:]]\+.*$,\1,g;T;p' "${MKIMAGE_LOG}")"
ATF_HEADER_OFFSET="$(sed -ne '0,/^Output:[[:blank:]]\+flash\.bin/p' "${MKIMAGE_LOG}" | awk '/CST: CONTAINER 0 offset:/{print $NF}')"
ATF_SIGNATURE_OFFSET="$(sed -ne '0,/^Output:[[:blank:]]\+flash\.bin/p' "${MKIMAGE_LOG}" | awk '/CST: CONTAINER 0: Signature Block:/{print $NF}')"
SPL_HEADER_OFFSET="$(sed -ne '/^Output:[[:blank:]]\+flash\.bin/,$p' "${MKIMAGE_LOG}" | awk '/CST: CONTAINER 0 offset:/{print $NF}')"
SPL_SIGNATURE_OFFSET="$(sed -ne '/^Output:[[:blank:]]\+flash\.bin/,$p' "${MKIMAGE_LOG}" | awk '/CST: CONTAINER 0: Signature Block:/{print $NF}')"

# The SPL-based U-Boot has three containers, the first one (for the SECO
# firmware) does not need signing, so the signing process involves two signing
# steps for the second and third container. That's why we need an intermediate
# (temporary) U-Boot image.
UBOOT_BIN="$(readlink -e "${1}")"
TARGET="$(readlink -f "${2}")"
TARGET_TMP="$(mktemp -t "$(basename "${UBOOT_BIN}")".XXXXXX)"

if [ "${ENCRYPT}" = "true" ]; then
	# Generate SPL container CSF descriptor
	CSF_UBOOT_SPL="csf_uboot_spl.txt"
	sed -e "s,%srk_table%,${SRK_TABLE},g" \
		-e "s,%cert_img%,${CERT_SRK},g" \
		-e "s,%key_index%,${CONFIG_KEY_INDEX},g" \
		-e "s,%srk_rvk_mask%,$(to_hex "${SRK_REVOKE_MASK}"),g" \
		-e "s,%u-boot-img%,${UBOOT_BIN},g" \
		-e "s,%container_offset%,${SPL_HEADER_OFFSET},g" \
		-e "s,%block_offset%,${SPL_SIGNATURE_OFFSET},g" \
		-e "s,%dek_len%,${dek_size},g" \
		-e "s,%dek_path%,${CONFIG_DEK_PATH},g" \
		"${SCRIPT_PATH}/csf_templates/encrypt_ahab_uboot" > "${CSF_UBOOT_SPL}"

	# Generate ATF container CSF descriptor
	CSF_UBOOT_ATF="csf_uboot_atf.txt"
	sed -e "s,%srk_table%,${SRK_TABLE},g" \
		-e "s,%cert_img%,${CERT_SRK},g" \
		-e "s,%key_index%,${CONFIG_KEY_INDEX},g" \
		-e "s,%srk_rvk_mask%,$(to_hex "${SRK_REVOKE_MASK}"),g" \
		-e "s,%u-boot-img%,${TARGET_TMP},g" \
		-e "s,%container_offset%,$(to_hex "$((ATF_CONTAINER_OFFSET*1024 + ATF_HEADER_OFFSET))"),g" \
		-e "s,%block_offset%,$(to_hex "$((ATF_CONTAINER_OFFSET*1024 + ATF_SIGNATURE_OFFSET))"),g" \
		-e "s,%dek_len%,${dek_size},g" \
		-e "s,%dek_path%,${CONFIG_DEK_PATH},g" \
		"${SCRIPT_PATH}/csf_templates/encrypt_ahab_uboot" > "${CSF_UBOOT_ATF}"
else
	# Generate SPL container CSF descriptor
	CSF_UBOOT_SPL="csf_uboot_spl.txt"
	sed -e "s,%srk_table%,${SRK_TABLE},g" \
		-e "s,%cert_img%,${CERT_SRK},g" \
		-e "s,%key_index%,${CONFIG_KEY_INDEX},g" \
		-e "s,%srk_rvk_mask%,$(to_hex "${SRK_REVOKE_MASK}"),g" \
		-e "s,%u-boot-img%,${UBOOT_BIN},g" \
		-e "s,%container_offset%,${SPL_HEADER_OFFSET},g" \
		-e "s,%block_offset%,${SPL_SIGNATURE_OFFSET},g" \
		"${SCRIPT_PATH}/csf_templates/sign_ahab_uboot" > "${CSF_UBOOT_SPL}"

	# Generate ATF container CSF descriptor
	CSF_UBOOT_ATF="csf_uboot_atf.txt"
	sed -e "s,%srk_table%,${SRK_TABLE},g" \
		-e "s,%cert_img%,${CERT_SRK},g" \
		-e "s,%key_index%,${CONFIG_KEY_INDEX},g" \
		-e "s,%srk_rvk_mask%,$(to_hex "${SRK_REVOKE_MASK}"),g" \
		-e "s,%u-boot-img%,${TARGET_TMP},g" \
		-e "s,%container_offset%,$(to_hex "$((ATF_CONTAINER_OFFSET*1024 + ATF_HEADER_OFFSET))"),g" \
		-e "s,%block_offset%,$(to_hex "$((ATF_CONTAINER_OFFSET*1024 + ATF_SIGNATURE_OFFSET))"),g" \
		"${SCRIPT_PATH}/csf_templates/sign_ahab_uboot" > "${CSF_UBOOT_ATF}"
fi
# Sign SPL container
if ! cst -i "${CSF_UBOOT_SPL}" -o "${TARGET_TMP}" >/dev/null; then
	echo "[ERROR] Could not sign SPL container"
	exit 1
fi

# Sign ATF container
if ! cst -i "${CSF_UBOOT_ATF}" -o "${TARGET}" >/dev/null; then
	echo "[ERROR] Could not sign ATF container"
	exit 1
fi

[ "${ENCRYPT}" = "true" ] && ENCRYPTED_MSG="and encrypted "
echo "Signed ${ENCRYPTED_MSG}image ready: ${TARGET}"
