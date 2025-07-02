#!/bin/bash
#===============================================================================
#
#  Copyright (C) 2015-2024 by Digi International Inc.
#  All rights reserved.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License version 2 as published by
#  the Free Software Foundation.
#
#  Description: U-Boot standalone build script
#
#  Parameters from the environment (not all needed):
#    DUB_GIT_URL:        U-Boot repo url
#    DUB_PLATFORMS:      Platforms to build
#    DUB_REVISION:       Revision of the U-Boot repository
#    DUB_TOOLCHAIN_URL:  Toolchain installer base url
#    DUB_TOOLCHAIN_DIR:  Toolchain environment folder path
#    UBOOT_DIR:          U-Boot source code path
#    WORKSPACE:          Base folder to build the binaries
#
#===============================================================================

# Exit when any command fails
set -e

# <platform> <uboot_make_target> <toolchain_type> <post-script>
while read -r pl mt tt ps; do
	AVAILABLE_PLATFORMS="${AVAILABLE_PLATFORMS:+${AVAILABLE_PLATFORMS} }${pl}"
	# Dashes are not allowed in variables so let's substitute them on
	# the fly with underscores.
	eval "${pl//-/_}_make_target=\"${mt}\""
	eval "${pl//-/_}_toolchain_type=\"${tt}\""
	eval "${pl//-/_}_post_script=\"${ps}\""
done<<-_EOF_
	ccmp15-dvk-512MB    "DEVICE_TREE=ccmp15-dvk-512MB"     cortexa7hf     "make_stm32_fip.sh"
	ccmp15-dvk-1GB      "DEVICE_TREE=ccmp15-dvk-1GB"       cortexa7hf     "make_stm32_fip.sh"
	ccmp13-dvk-256MB    "DEVICE_TREE=ccmp13-dvk-256MB"     cortexa7hf     "make_stm32_fip.sh"
_EOF_

# Set default values if not provided by user
DUB_GIT_URL="${DUB_GIT_URL:-https://github.com/digi-embedded/u-boot.git}"
DUB_TOOLCHAIN_URL="${DUB_TOOLCHAIN_URL:-http://10.101.8.63/exports/tftpboot/toolchain}"
DUB_PLATFORMS="${DUB_PLATFORMS:-${AVAILABLE_PLATFORMS}}"

clone_uboot_repo()
{
	if [ ! -d "${UBOOT_DIR}/.git" ]; then
		echo "- Clone U-Boot repository:"
		git clone "${DUB_GIT_URL}" "${UBOOT_DIR}"
	fi

	(
		cd "${UBOOT_DIR}" || exit 1
		git clean -ffdx && git restore .
		echo "- Update U-Boot repository:"
		git pull "$(git remote)"
		git show-ref --verify --quiet refs/tags/"${DUB_REVISION}" || DUB_REMOTE="$(git remote)/"
		git -c core.fsync=loose-object -c gc.autoDetach=false -c core.pager=cat checkout -B "${DUB_REVISION}" "${DUB_REMOTE}${DUB_REVISION}"
		unset DUB_REMOTE
	)
}

error() {
	printf "%s\n" "${1}"
	exit 1
}

# Sanity check (User environment)
[ -z "${WORKSPACE}" ] && error "WORKSPACE not specified"

# Unset BUILD_TAG from Jenkins so U-Boot does not show it
unset BUILD_TAG

# Remove nested directories from the revision
DUB_REVISION_SANE="$(echo "${DUB_REVISION}" | tr '/' '_')"

DUB_IMGS_DIR="${WORKSPACE}/images"
DUB_TOOLCHAIN_DIR="${DUB_TOOLCHAIN_DIR:-${WORKSPACE}/sdk}"
UBOOT_DIR="${UBOOT_DIR:-${WORKSPACE}/u-boot${DUB_REVISION_SANE:+-${DUB_REVISION_SANE}}.git}"
rm -rf "${DUB_IMGS_DIR}"
mkdir -p "${DUB_IMGS_DIR}" "${DUB_TOOLCHAIN_DIR}"

if [ -z "${DUB_REVISION}" ]; then
	echo "- Using local U-Boot source ( ${UBOOT_DIR} )"
	DUB_REVISION="LOCAL"
else
	# Clone and Update U-Boot repository
	clone_uboot_repo
fi

CPUS="$(echo /sys/devices/system/cpu/cpu[0-9]* | wc -w)"
[ "${CPUS}" -gt 1 ] && MAKE_JOBS="-j${CPUS}"
MAKE="make ${MAKE_JOBS}"

printf "\n[INFO] Build U-Boot revision \"%s\" for platforms \"%s\"\n\n" "${DUB_REVISION}" "${DUB_PLATFORMS}"

for platform in ${DUB_PLATFORMS}; do
	# Build in a sub-shell to avoid mixing environments for different platform
	(
		cd "${UBOOT_DIR}" || exit 1
		printf "\n[INFO] [PLATFORM: %s - CPUS: %s]\n" "${platform}" "${CPUS}"

		# Install toolchain
		eval "TOOLCHAIN_TYPE=\"\${${platform//-/_}_toolchain_type}\""
		for TLABEL in ${DUB_REVISION_SANE}-${platform} ${DUB_REVISION_SANE} ${platform} default; do
			TLABEL="${TLABEL}-${TOOLCHAIN_TYPE}"

			# If the toolchain is already installed exit the loop
			if [ -d "${DUB_TOOLCHAIN_DIR}/${TLABEL}" ]; then
				printf "\n[INFO] toolchain '${TLABEL}' is already installed\n\n"
				break
			fi

			if wget -q --spider "${DUB_TOOLCHAIN_URL}/toolchain-${TLABEL}.sh"; then
				tmp_toolchain="$(mktemp /tmp/toolchain.XXXXXX)"
				printf "\n[INFO] Downloading toolchain from ${DUB_TOOLCHAIN_URL}/toolchain-${TLABEL}.sh\n\n"
				wget -q -O "${tmp_toolchain}" "${DUB_TOOLCHAIN_URL}/toolchain-${TLABEL}.sh" && chmod +x "${tmp_toolchain}"
				printf "\n[INFO] Installing toolchain-%s.sh\n\n" "${TLABEL}"
				rm -rf "${DUB_TOOLCHAIN_DIR}"/"${TLABEL:?}" && sh "${tmp_toolchain}" -y -d "${DUB_TOOLCHAIN_DIR}"/"${TLABEL}"
				rm -f "${tmp_toolchain}"
				break
			fi
		done

		eval "$(grep "^export CROSS_COMPILE=" "${DUB_TOOLCHAIN_DIR}"/"${TLABEL}"/environment-setup-*)"
		eval "$(grep "^export PATH=" "${DUB_TOOLCHAIN_DIR}"/"${TLABEL}"/environment-setup-*)"
		eval "$(grep "^export SDKTARGETSYSROOT=" "${DUB_TOOLCHAIN_DIR}"/"${TLABEL}"/environment-setup-*)"
		eval "$(grep "^export KCFLAGS=" "${DUB_TOOLCHAIN_DIR}"/"${TLABEL}"/environment-setup-*)"

		eval "UBOOT_MAKE_TARGET=\"\${${platform//-/_}_make_target}\""
		UBOOT_SHA1=$(git log --pretty=format:"%H" -1)

		printf "\n[INFO] Build U-Boot for target '${UBOOT_MAKE_TARGET}' (commit ${UBOOT_SHA1})...\n"
		${MAKE} distclean
		${MAKE} "${platform%-*}"_defconfig
		${MAKE} "${UBOOT_MAKE_TARGET}"

		eval "BOOT_POST_SCRIPT=\"\${${platform//-/_}_post_script}\""
		if [ -z "${BOOT_POST_SCRIPT}" ]; then
			# Copy u-boot image
			cp --remove-destination "${UBOOT_MAKE_TARGET}" "${DUB_IMGS_DIR}"/"${UBOOT_MAKE_TARGET/u-boot/u-boot-${platform}}"
		else
			# Some extra environment needed to build boot artifacts
			export TOOLCHAIN=$(find ${DUB_TOOLCHAIN_DIR}/${TLABEL} -type f -name "environment-setup-*")
			export PLATFORM_NAME=${platform}
			export UBOOT_DIR=${UBOOT_DIR}

			# Build the boot artifacts
			( cd tools/digi && git clean -ffdx )
			./tools/digi/"${BOOT_POST_SCRIPT}"

			# Copy boot artifacts
			mkdir -p "${DUB_IMGS_DIR}/${platform}"
			cp --remove-destination ${WORKSPACE}/output/* "${DUB_IMGS_DIR}"/"${platform}"/
			printf "\n[INFO] Boot artifacts for %s are available in %s/%s\n" "${platform}" "${DUB_IMGS_DIR}" "${platform}"
		fi
	)
done
