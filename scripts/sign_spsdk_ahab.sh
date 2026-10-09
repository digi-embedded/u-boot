#!/bin/sh
# SPDX-License-Identifier: MPL-2.0
#
# Copyright 2026 Digi International Inc.
#
# Description:
#   Sign AHAB based bootloader using SPSDK and generate classic and PQC SRK
#   efuse binaries.
#
#   Following variables are used (from environment or '.config' file)
#      CONFIG_SIGN_KEYS_PATH: [mandatory] path for the SPSDK AHAB PKI tree
#                             containing classic/ and pqc/ directories.
#      CONFIG_KEY_INDEX: [optional] key index to use. Default is 0.
#      SRK_REVOKE_MASK: [optional], bitmask of the revoked SRKs.
#      ENABLE_ENCRYPTION: [not supported yet] requesting encryption fails.
#      CONFIG_DEK_PATH: reserved for future encryption support.
#

die() {
	printf '[ERROR] %s\n' "$*" >&2
	exit 1
}

# Avoid parallel execution of this script
unset TMP_AHAB_SIGN_CONF LOCK_HELD
SINGLE_PROCESS_LOCK="/tmp/sign_script.lock.d"
cleanup_and_exit() {
	[ -z "${TMP_AHAB_SIGN_CONF}" ] || rm -f -- "${TMP_AHAB_SIGN_CONF}"
	[ -n "${LOCK_HELD}" ] && rmdir -- "${SINGLE_PROCESS_LOCK}" 2>/dev/null || true
}
trap 'cleanup_and_exit' EXIT
trap 'exit 1' INT TERM
while ! mkdir "${SINGLE_PROCESS_LOCK}" > /dev/null 2>&1; do
	sleep 1
done
LOCK_HELD="true"

SCRIPT_NAME="$(basename "${0}")"
SCRIPT_PATH="$(cd "$(dirname "${0}")" && pwd)"

# Defaults for optional parameters
CONFIG_KEY_INDEX="${CONFIG_KEY_INDEX:-0}"
SRK_REVOKE_MASK="${SRK_REVOKE_MASK:-0x00}"

CLASSIC_KEY_DIR="${CONFIG_SIGN_KEYS_PATH}/classic/keys"
PQC_KEY_DIR="${CONFIG_SIGN_KEYS_PATH}/pqc/keys"
AHAB_SIGN_TEMPLATE="${SCRIPT_PATH}/spsdk/ahab_sign_template.yaml"

#
# Sanity checks
#
[ "${#}" != "2" ] && die "Usage: ${SCRIPT_NAME} <input-unsigned-bootloader> <output-signed-bootloader>"
[ -z "${CONFIG_SIGN_KEYS_PATH}" ] && die "Undefined CONFIG_SIGN_KEYS_PATH"
[ -d "${CLASSIC_KEY_DIR}" ] || die "Classic PKI key directory not found: ${CLASSIC_KEY_DIR}"
[ -d "${PQC_KEY_DIR}" ] || die "PQC PKI key directory not found: ${PQC_KEY_DIR}"
[ -r "${AHAB_SIGN_TEMPLATE}" ] || die "Missing signing template: ${AHAB_SIGN_TEMPLATE}"
# Key index within supported range
printf '%s\n' "${CONFIG_KEY_INDEX}" | grep -qs '^[0-3]$' || die "Invalid CONFIG_KEY_INDEX: ${CONFIG_KEY_INDEX} (expected 0 to 3)"
# SRK with correct format and revoking key 3 not allowed
printf '%s\n' "${SRK_REVOKE_MASK}" | grep -qs '^0x0\?[0-9a-fA-F]$' || die "Invalid SRK_REVOKE_MASK: ${SRK_REVOKE_MASK}"
[ "$((SRK_REVOKE_MASK & 0x8))" -eq 0 ] || die "SRK 3 cannot be revoked"
[ "$((SRK_REVOKE_MASK & (1 << CONFIG_KEY_INDEX)))" -eq 0 ] || die "CONFIG_KEY_INDEX selects a revoked SRK"
# SPSDK required commands
command -v nxpimage >/dev/null 2>&1 || die "nxpimage is required (install spsdk)"
command -v nxpcrypto >/dev/null 2>&1 || die "nxpcrypto is required (install spsdk)"

IMXBOOT_BIN="$(readlink -e "${1}")"
IMXBOOT_BIN_SIGNED="$(readlink -f "${2}")"

OUTPUT_DIR="$(dirname -- "${IMXBOOT_BIN_SIGNED}")"
TMP_AHAB_SIGN_CONF="$(mktemp -t ahab_sign_conf.XXXXXX.yaml)"

CLASSIC_SIGNER="$(echo "${CLASSIC_KEY_DIR}"/SRK"${CONFIG_KEY_INDEX}"_*_key.pem)"
PQC_SIGNER="$(echo "${PQC_KEY_DIR}"/SRK"${CONFIG_KEY_INDEX}"_*_key.pem)"
CLASSIC_PUB_0="$(echo "${CLASSIC_KEY_DIR}"/SRK0_*_key.pub)"
CLASSIC_PUB_1="$(echo "${CLASSIC_KEY_DIR}"/SRK1_*_key.pub)"
CLASSIC_PUB_2="$(echo "${CLASSIC_KEY_DIR}"/SRK2_*_key.pub)"
CLASSIC_PUB_3="$(echo "${CLASSIC_KEY_DIR}"/SRK3_*_key.pub)"
PQC_PUB_0="$(echo "${PQC_KEY_DIR}"/SRK0_*_key.pub.pem)"
PQC_PUB_1="$(echo "${PQC_KEY_DIR}"/SRK1_*_key.pub.pem)"
PQC_PUB_2="$(echo "${PQC_KEY_DIR}"/SRK2_*_key.pub.pem)"
PQC_PUB_3="$(echo "${PQC_KEY_DIR}"/SRK3_*_key.pub.pem)"

nxpcrypto rot calculate-hash -f mimx9596 \
	-k "${CLASSIC_PUB_0}" -k "${CLASSIC_PUB_1}" \
	-k "${CLASSIC_PUB_2}" -k "${CLASSIC_PUB_3}" \
	-o "${OUTPUT_DIR}/SRK_efuses_classic.bin" || die "Cannot generate SRK_efuses_classic.bin"

nxpcrypto rot calculate-hash -f mimx9596 \
	-k "${PQC_PUB_0}" -k "${PQC_PUB_1}" \
	-k "${PQC_PUB_2}" -k "${PQC_PUB_3}" \
	-o "${OUTPUT_DIR}/SRK_efuses_pqc.bin" || die "Cannot generate SRK_efuses_pqc.bin"

sed \
	-e "s|@KEY_INDEX@|${CONFIG_KEY_INDEX}|g" \
	-e "s|@REVOKE_MASK@|'${SRK_REVOKE_MASK}'|g" \
	-e "s|@CLASSIC_SIGNER@|'${CLASSIC_SIGNER}'|g" \
	-e "s|@PQC_SIGNER@|'${PQC_SIGNER}'|g" \
	-e "s|@CLASSIC_PUB_0@|'${CLASSIC_PUB_0}'|g" \
	-e "s|@CLASSIC_PUB_1@|'${CLASSIC_PUB_1}'|g" \
	-e "s|@CLASSIC_PUB_2@|'${CLASSIC_PUB_2}'|g" \
	-e "s|@CLASSIC_PUB_3@|'${CLASSIC_PUB_3}'|g" \
	-e "s|@PQC_PUB_0@|'${PQC_PUB_0}'|g" \
	-e "s|@PQC_PUB_1@|'${PQC_PUB_1}'|g" \
	-e "s|@PQC_PUB_2@|'${PQC_PUB_2}'|g" \
	-e "s|@PQC_PUB_3@|'${PQC_PUB_3}'|g" \
	"${AHAB_SIGN_TEMPLATE}" > "${TMP_AHAB_SIGN_CONF}" || die "Cannot prepare signing configuration"

nxpimage ahab sign -c "${TMP_AHAB_SIGN_CONF}" -b "${IMXBOOT_BIN}" -o "${IMXBOOT_BIN_SIGNED}" -fs "${OUTPUT_DIR}/fuse-script" -m emmc_boot || die "SPSDK signing failed"
[ -s "${IMXBOOT_BIN_SIGNED}" ] || die "SPSDK did not create a signed image"
printf 'Signed image ready: %s\n' "${IMXBOOT_BIN_SIGNED}"
