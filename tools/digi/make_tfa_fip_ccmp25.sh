#!/bin/sh
#===============================================================================
#
#  Copyright (C) 2025 by Digi International Inc.
#  All rights reserved.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License version 2 as published by
#  the Free Software Foundation.
#
#  Description: TF-A + FIP Boot standalone build script
#
#  Parameters from the environment (Optional):
#    ATF_BRANCH:         Branch where TF-A repository will be cloned from
#    ATF_REPO:           URL where TF-A repository will be cloned from
#    ATF_REV:            Revision where TF-A repository will be cloned from
#    OPTEE_BRANCH:       Branch where OP-TEE repository will be cloned from
#    OPTEE_REPO:         URL where OP-TEE repository will be cloned from
#    OPTEE_REV:          Revision where OP-TEE repository will be cloned from
#    OUTPUT_PATH:        Output folder for built binaries
#    UBOOT_DIR:          U-Boot source code path
#    WORKSPACE:          Base folder to build the binaries
#===============================================================================

# Exit when any command fails
set -e

usage() {
        cat <<EOF

Usage: $(basename "$0") [OPTIONS]

    -h,--help                           Show help

Generate bootloader binaries for STM devices.

EOF
}

#
# Check cross-compiler existence
#
validate_environment()
{
	if [ -n "${CROSS_COMPILE}" ]; then
		type "${CROSS_COMPILE}"gcc >/dev/null && return
	fi

	# Give priority to a DEY toolchain
	CROSS_COMPILE="aarch64-dey-linux-"
	if ! type ${CROSS_COMPILE}gcc >/dev/null; then
		CROSS_COMPILE="aarch64-linux-"
		if ! type ${CROSS_COMPILE}gcc >/dev/null; then
			echo "[ERROR] Missing cross-compiler"
			exit 1
		fi
	fi
}

clone_optee_repo()
{
	if [ ! -d "${OPTEE_DIR}" ]; then
		echo "- Clone optee-os repository:"
		git clone "${OPTEE_REPO}" -b "${OPTEE_BRANCH}" "${OPTEE_DIR}"
	fi

	(
		cd "${OPTEE_DIR}" || { echo "clone_optee_repo: OPTEE_DIR not found"; exit 1; }
		git clean -ffdx && git restore .
		echo "- Update optee-os repository:"
		git fetch "$(git remote)"
		git -c core.fsync=loose-object -c gc.autoDetach=false -c core.pager=cat checkout -B "${OPTEE_BRANCH}" "${OPTEE_REV}"
	)
}

patch_optee_repo()
{
	if [ ! -d "${OPTEE_DIR}" ]; then
		echo "- Missing optee-os repository"
		exit 1
	fi

	echo "- Patch optee-os repository:"
	(
		cd "${OPTEE_DIR}" || exit 1
		for p in ${OPTEE_PATCHES}; do
			echo "- Apply patch: ${p}"
			patch -p1 < "${WORKSPACE}"/patch/optee/"${p}" || exit 2
		done
	)
}


build_optee()
{
	echo "- Build OPTEE binary for: ccmp25-dvk"
	(
		cd "${OPTEE_DIR}" || { echo "build_optee: OPTEE_DIR not found"; exit 1; }
		${MAKE} PLATFORM=stm32mp2 \
			CROSS_COMPILE_core="${CROSS_COMPILE}" \
			ARCH=arm \
			CFG_ARM64_core=y \
			CROSS_COMPILE_ta_arm64="${CROSS_COMPILE}" \
			NOWERROR=1 \
			LDFLAGS= \
			CFG_TEE_CORE_LOG_LEVEL=1 \
			CFG_TEE_CORE_DEBUG=y \
			CFG_EMBED_DTB_SOURCE_FILE=ccmp25-dvk.dts \
			CFG_STM32MP_PROFILE=secure_and_system_services \
			O="${OPTEE_DIR}/build/optee-ccmp25-dvk"
	)
}

clone_atf_repo()
{
	if [ ! -d "${ATF_DIR}" ]; then
		echo "- Clone arm-trusted-firmware repository:"
		git clone "${ATF_REPO}" -b "${ATF_BRANCH}" "${ATF_DIR}"
	fi

	(
		cd "${ATF_DIR}" || exit 1
		git clean -ffdx && git restore .
		echo "- Update arm-trusted-firmware repository:"
		git fetch "$(git remote)"
		git -c core.fsync=loose-object -c gc.autoDetach=false -c core.pager=cat checkout -B "${ATF_BRANCH}" "${ATF_REV}"
	)
}

patch_atf_repo()
{
	if [ ! -d "${ATF_DIR}" ]; then
		echo "- Missing arm-trusted-firmware repository"
		exit 1
	fi

	echo "- Patch arm-trusted-firmware repository:"
	(
		cd "${ATF_DIR}" || exit 1
		for p in ${ATF_PATCHES}; do
			echo "- Apply patch: ${p}"
			patch -p1 < "${WORKSPACE}"/patch/atf/"${p}" || exit 2
		done
	)
}

build_atf()
{
	echo "- Build ATF binary for: ccmp25-dvk"
	mkdir -p "${OUTPUT_PATH}"
	(
		cd "${ATF_DIR}" || { echo "build_atf: ATF_DIR not found"; exit 1; }

		${MAKE} realclean

		for config in emmc usb; do
			echo "-- Building for ${config}..."

			if [ "${config}" = "emmc" ]; then
				EXTRA_FLAGS="PSA_FWU_SUPPORT=1 STM32MP_EMMC=1"
			else
				EXTRA_FLAGS="STM32MP_USB_PROGRAMMER=1"
			fi

			# Build dtb binaries
			${MAKE} PLAT=stm32mp2 \
				ARCH=aarch64 \
				ARM_ARCH_MAJOR=8 \
				CROSS_COMPILE="${CROSS_COMPILE}" \
				LOG_LEVEL=20 \
				BUILD_PLAT="${ATF_DIR}/build/optee-${config}-ccmp25-dvk" \
				DTB_FILE_NAME=ccmp25-dvk.dtb \
				SPD=opteed \
				STM32MP25=1 \
				${EXTRA_FLAGS} \
				dtbs

			# Build ATF binary
			${MAKE} PLAT=stm32mp2 \
				ARCH=aarch64 \
				ARM_ARCH_MAJOR=8 \
				CROSS_COMPILE="${CROSS_COMPILE}" \
				LOG_LEVEL=20 \
				BUILD_PLAT="${ATF_DIR}/build/optee-${config}-ccmp25-dvk" \
				DTB_FILE_NAME=ccmp25-dvk.dtb \
				SPD=opteed \
				STM32MP25=1 \
				STM32MP_DDR4_TYPE=1 \
				${EXTRA_FLAGS} \
				all

			cp --remove-destination \
				"${ATF_DIR}/build/optee-${config}-ccmp25-dvk/tf-a-ccmp25-dvk.stm32" \
				"${OUTPUT_PATH}/tf-a-ccmp25-dvk-optee-${config}.stm32"
		done
	)
}

build_fip()
{
	echo "- Build FIP binaries for: ccmp25-dvk"
	(
		cd "${ATF_DIR}" || { echo "build_fip: ATF_DIR not found"; exit 1; }

		for config in emmc usb; do
			echo "-- Building for ${config}..."

			# Build DDR binary
			fiptool create \
				--ddr-fw "${ATF_DIR}/drivers/st/ddr/phy/firmware/bin/stm32mp2/ddr4_pmu_train.bin" \
				"${OUTPUT_PATH}/fip-ccmp25-dvk-ddr-optee-${config}.bin"

			# Build FIP binary
			fiptool create \
				--fw-config "${ATF_DIR}/build/optee-${config}-ccmp25-dvk/fdts/ccmp25-dvk-fw-config.dtb" \
				--soc-fw "${ATF_DIR}/build/optee-${config}-ccmp25-dvk/bl31.bin" \
				--soc-fw-config "${ATF_DIR}/build/optee-${config}-ccmp25-dvk/fdts/ccmp25-dvk-bl31.dtb" \
				--ddr-fw "${ATF_DIR}/drivers/st/ddr/phy/firmware/bin/stm32mp2/ddr4_pmu_train.bin" \
				--tos-fw "${OPTEE_DIR}/build/optee-ccmp25-dvk/core/tee-header_v2.bin" \
				--tos-fw-extra1 "${OPTEE_DIR}/build/optee-ccmp25-dvk/core/tee-pager_v2.bin" \
				--tos-fw-extra2 "${OPTEE_DIR}/build/optee-ccmp25-dvk/core/tee-pageable_v2.bin" \
				--nt-fw "${UBOOT_DIR}/u-boot-nodtb.bin" \
				--hw-config "${UBOOT_DIR}/u-boot.dtb" \
				"${OUTPUT_PATH}/fip-ccmp25-dvk-optee-${config}.bin"
		done
	)
}

##### Main
WORKSPACE="${WORKSPACE:-$( cd "$( dirname "$0" )" && pwd )}"

OPTEE_REPO="${OPTEE_REPO:-https://github.com/digi-embedded/optee_os.git}"
OPTEE_BRANCH="${OPTEE_BRANCH:-4.0.0/stm/maint}"
# Tag: dey-5.0-r2.2-v4.0
OPTEE_REV="${OPTEE_REV:-da06ccf751938a642805f11b7aae3d7cde5f6917}"
OPTEE_DIR="${WORKSPACE}/optee_os"
OPTEE_PATCHES=""

ATF_REPO="${ATF_REPO:-https://github.com/digi-embedded/arm-trusted-firmware.git}"
ATF_BRANCH="${ATF_BRANCH:-v2.10/stm32mp/maint}"
# Tag: dey-5.0-r2.2-v2.10
ATF_REV="${ATF_REV:-e22f7d89868db1d909d582be9bdd73679648a438}"
ATF_DIR="${WORKSPACE}/arm-trusted-firmware"
ATF_PATCHES=""

OUTPUT_PATH="${OUTPUT_PATH:-${WORKSPACE}/output}"
UBOOT_DIR="${UBOOT_DIR:-$(realpath "${WORKSPACE}"/../..)}"

# Parse command line arguments
while [ "${1}" != "" ]; do
	case ${1} in
		-h|--help )		usage; exit;;
		*) 			echo "[ERROR] Unknown option"; usage; exit 1;;
	esac
	shift
done

CPUS="$(echo /sys/devices/system/cpu/cpu[0-9]* | wc -w)"
[ "${CPUS}" -gt 1 ] && MAKE_JOBS="-j${CPUS}"
MAKE="make ${MAKE_JOBS}"

validate_environment
clone_optee_repo
patch_optee_repo
build_optee
clone_atf_repo
patch_atf_repo
build_atf
build_fip
