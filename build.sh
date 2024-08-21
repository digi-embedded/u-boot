#!/bin/bash
#===============================================================================
#
#  Copyright (C) 2015-2023 by Digi International Inc.
#  All rights reserved.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License version 2 as published by
#  the Free Software Foundation.
#
#  Description: U-Boot jenkins build script
#
#  Parameters from the environment (not all needed):
#    DUB_PLATFORMS:      Platforms to build
#    DUB_REVISION:       Revision of the U-Boot repository
#    DUB_GIT_URL:        U-Boot repo url
#    DUB_TOOLCHAIN_URL:  Toolchain installer base url
#
#===============================================================================

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
	ccimx91-dvk             all               aarch64       "make_imxboot_ccimx91.sh"
_EOF_

# Set default values if not provided by Jenkins
DUB_GIT_URL="${DUB_GIT_URL:-ssh://git@stash.digi.com/uboot/u-boot-denx.git}"
DUB_TOOLCHAIN_URL="${DUB_TOOLCHAIN_URL:-http://10.101.8.63/exports/tftpboot/toolchain}"
DUB_PLATFORMS="${DUB_PLATFORMS:-${AVAILABLE_PLATFORMS}}"

clone_uboot_repo()
{
	if [ ! -d "${DUB_UBOOT_DIR}/.git" ]; then
		echo "- Clone U-Boot repository:"
		git clone "${DUB_GIT_URL}" "${DUB_UBOOT_DIR}"
	fi

	(
		cd "${DUB_UBOOT_DIR}" || exit 1
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

# Sanity check (Jenkins environment)
[ -z "${WORKSPACE}" ] && error "WORKSPACE not specified"
[ -z "${DUB_REVISION}" ] && error "DUB_REVISION not specified"

# Unset BUILD_TAG from Jenkins so U-Boot does not show it
unset BUILD_TAG

printf "\n[INFO] Build U-Boot \"%s\" for \"%s\"\n\n" "${DUB_REVISION}" "${DUB_PLATFORMS}"

# Remove nested directories from the revision
DUB_REVISION_SANE="$(echo "${DUB_REVISION}" | tr '/' '_')"

DUB_IMGS_DIR="${WORKSPACE}/images"
DUB_TOOLCHAIN_DIR="${WORKSPACE}/sdk"
DUB_UBOOT_DIR="${WORKSPACE}/u-boot${DUB_REVISION_SANE:+-${DUB_REVISION_SANE}}.git"
rm -rf "${DUB_IMGS_DIR}" "${DUB_TOOLCHAIN_DIR}" "${DUB_UBOOT_DIR}"
mkdir -p "${DUB_IMGS_DIR}" "${DUB_TOOLCHAIN_DIR}"

# clone/update U-Boot repository
clone_uboot_repo

CPUS="$(echo /sys/devices/system/cpu/cpu[0-9]* | wc -w)"
[ "${CPUS}" -gt 1 ] && MAKE_JOBS="-j${CPUS}"
MAKE="make ${MAKE_JOBS}"
WGET="wget -q --timeout=30 --tries=3"

for platform in ${DUB_PLATFORMS}; do
	# Build in a sub-shell to avoid mixing environments for different platform
	(
		cd "${DUB_UBOOT_DIR}" || exit 1
		printf "\n[PLATFORM: %s - CPUS: %s]\n" "${platform}" "${CPUS}"

		# Install toolchain
		eval "TOOLCHAIN_TYPE=\"\${${platform//-/_}_toolchain_type}\""
		for TLABEL in ${DUB_REVISION_SANE}-${platform} ${DUB_REVISION_SANE} ${platform} default; do
			TLABEL="${TLABEL}-${TOOLCHAIN_TYPE}"
			# If the toolchain is already installed exit the loop
			[ -d "${DUB_TOOLCHAIN_DIR}/${TLABEL}" ] && break
			if ${WGET} --spider "${DUB_TOOLCHAIN_URL}/toolchain-${TLABEL}.sh"; then
				printf "\n[INFO] Installing toolchain-%s.sh\n\n" "${TLABEL}"
				tmp_toolchain="$(mktemp /tmp/toolchain.XXXXXX)"
				${WGET} -O "${tmp_toolchain}" "${DUB_TOOLCHAIN_URL}/toolchain-${TLABEL}.sh" && chmod +x "${tmp_toolchain}"
				rm -rf "${DUB_TOOLCHAIN_DIR}"/"${TLABEL:?}" && sh "${tmp_toolchain}" -y -d "${DUB_TOOLCHAIN_DIR}"/"${TLABEL}"
				rm -f "${tmp_toolchain}"
				break
			fi
		done

		[ -d "${DUB_TOOLCHAIN_DIR}/${TLABEL}" ] || error "No toolchain available"

		eval "$(grep "^export CROSS_COMPILE=" "${DUB_TOOLCHAIN_DIR}"/"${TLABEL}"/environment-setup-*)"
		eval "$(grep "^export PATH=" "${DUB_TOOLCHAIN_DIR}"/"${TLABEL}"/environment-setup-*)"
		eval "$(grep "^export SDKTARGETSYSROOT=" "${DUB_TOOLCHAIN_DIR}"/"${TLABEL}"/environment-setup-*)"
		eval "$(grep "^export KCFLAGS=" "${DUB_TOOLCHAIN_DIR}"/"${TLABEL}"/environment-setup-*)"

		eval "UBOOT_MAKE_TARGET=\"\${${platform//-/_}_make_target}\""
		${MAKE} distclean
		${MAKE} "${platform}"_defconfig
		${MAKE} "${UBOOT_MAKE_TARGET}"

		eval "BOOT_POST_SCRIPT=\"\${${platform//-/_}_post_script}\""
		if [ -z "${BOOT_POST_SCRIPT}" ]; then
			# Copy u-boot image
			UBOOT_FINAL_NAME="$(echo "${UBOOT_MAKE_TARGET}" | sed -e "s,-dtb,,g" -e "s,u-boot,u-boot-${platform},g")"
			mkdir -p "${DUB_IMGS_DIR}/${platform}"
			cp --remove-destination "${UBOOT_MAKE_TARGET}" "${DUB_IMGS_DIR}"/"${platform}"/"${UBOOT_FINAL_NAME}"
		else
			# Some extra environment needed to build optee-os
			eval "$(grep "^export OECORE_NATIVE_SYSROOT=" "${DUB_TOOLCHAIN_DIR}"/"${TLABEL}"/environment-setup-*)"
			export OPENSSL_MODULES="${OECORE_NATIVE_SYSROOT}/usr/lib/ossl-modules"
			export LIBGCC_LOCATE_CFLAGS="--sysroot=${SDKTARGETSYSROOT}"
			# Build the boot artifacts
			( cd tools/digi && git clean -ffdx )
			./tools/digi/"${BOOT_POST_SCRIPT}"
			# Copy boot artifacts
			mkdir -p "${DUB_IMGS_DIR}/${platform}"
			\cp --remove-destination tools/digi/output/* "${DUB_IMGS_DIR}"/"${platform}"/
		fi
	)
done
