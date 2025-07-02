#!/bin/sh
#===============================================================================
#
#  Copyright (C) 2023 by Digi International Inc.
#  All rights reserved.
#
#  This program is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License version 2 as published by
#  the Free Software Foundation.
#
#  Description: FIP Boot standalone build script
#
#  Parameters from the environment (Optional):
#    ATF_DIR:            Path to save TF-A repository
#    ATF_REPO:           URL from clone TF-A repository
#    ATF_REVISION:       Revision of the TF-A repository
#    ATF_PATCHES:        Patches list to apply TF-A code
#    OPTEE_DIR:          Path to save OP-TEE repository
#    OPTEE_REPO:         URL from clone OP-TEE repository
#    OPTEE_REVISION:     Revision of the OP-TEE repository
#    OPTEE_PATCHES:      Patches list to apply OP-TEE code
#    OUTPUT_PATH:        Output folder to built binaries
#    PLATFORM_NAME:      Platform name to build
#    TOOLCHAIN:          Environment toolchain path file
#    UBOOT_DIR:          U-Boot source code path
#    WORKSPACE:          Base folder to build the binaries
#===============================================================================

# Exit when any command fails
set -e

usage() {
        cat <<EOF

Usage: $(basename "$0") [OPTIONS]

    -d,--debug                          Enable debug build
    -o,--output                         Output folder path for built binary files
    -p,--platform <name>                Platform name to build
    -t,--toolchain <file path>          Environment toolchain path file
    -u,--uboot <u-boot directory>       U-Boot build directory
    -h,--help                           Show help

EOF
}

#
# Check and prepare environment
#
validate_environment()
{
	echo -n "- Verify environment:"
	if [ ! -f "${TOOLCHAIN}" ]; then
		echo " Missing Environment toolchain file"
		exit 1
	else
		echo " OK"
	fi

	# Clean output directory
	rm -rf "${OUTPUT_PATH}"

	# Create directories
	mkdir -p "${OUTPUT_PATH}"
}

clone_atf_repo()
{
	if [ ! -e ${ATF_DIR} ]; then
		echo "- Clone arm-trusted-firmware repository:"
		git clone ${ATF_REPO} ${ATF_DIR} && echo " OK" || exit 1
	fi

	cd ${ATF_DIR}
	git fetch --prune --all && git fetch -t --all
	git checkout ${ATF_REVISION}
	echo "- Update arm-trusted-firmware repository:"
	if $(git symbolic-ref -q HEAD >/dev/null); then
		git pull
	fi
	git clean -f -d
	cd -
}

clone_optee_repo()
{
	if [ ! -e ${OPTEE_DIR} ]; then
		echo "- Clone OPTEE repository:"
		git clone ${OPTEE_REPO} ${OPTEE_DIR} && echo " OK" || exit 1
		echo " OK"
	fi

	cd ${OPTEE_DIR}
	git fetch --prune --all && git fetch -t --all
	git checkout ${OPTEE_REVISION}
	echo "- Update OPTEE repository:"
	if $(git symbolic-ref -q HEAD >/dev/null); then
		git pull
	fi
	git clean -f -d
	cd -
}

build_optee()
{
	cd ${OPTEE_DIR}
	echo ""
	echo " ************************************** "
	OPTEE_SHA1=$(git log --pretty=format:"%H" -1)
	echo "- Build OPTEE binary for: ${PLATFORM_NAME} (commit ${OPTEE_SHA1})\n"

	# Source toolchain
	. ${TOOLCHAIN}
	# Clear LDFLAGS to avoid the option -Wl recognize issue
	unset CFLAGS
	unset LDFLAGS
	# Make sure source code is clean
	make clean

	if [ "${DEBUG_BUILD}" -eq "0" ]; then
		echo "- Execute make for ${PLATFORM_NAME}"
		# Release command
		make -j8 PLATFORM=stm32mp1 CFG_EMBED_DTB_SOURCE_FILE=${PLATFORM_NAME}.dts O=build all
	else
		echo "- Execute make in DEBUG mode for ${PLATFORM_NAME}"
		# Debug command
		make -j8 PLATFORM=stm32mp1 CFG_EMBED_DTB_SOURCE_FILE=${PLATFORM_NAME}.dts CFG_TEE_CORE_LOG_LEVEL=3 CFG_TEE_TA_LOG_LEVEL=2 CFG_STM32_EARLY_CONSOLE_UART=${OPTEE_CFG_STM32_EARLY_CONSOLE_UART} CFG_UNWIND=y O=build all
	fi

	if [ "$?" -eq "0" ]; then
		echo " OPTEE binary generated successfully"
	else
		echo " [ERROR] an unexpected error occurred building the OPTEE binary"
		exit 1
	fi
}

build_atf()
{
	cd ${ATF_DIR}
	echo ""
	echo " ************************************** "
	ATF_SHA1=$(git log --pretty=format:"%H" -1)
	echo "- Build arm-trusted-firmware for: ${PLATFORM_NAME} (commit ${ATF_SHA1})\n"

	# Source toolchain
	. ${TOOLCHAIN}
	# Clear LDFLAGS to avoid the option -Wl recognize issue
	unset LDFLAGS
	# Make sure source code is clean
	make realclean

	#### NAND + USB ####
	echo ""
	echo "- Build TF-A binary for: ${PLATFORM_NAME}"
	make ARM_ARCH_MAJOR=7 ARCH=aarch32 PLAT=stm32mp1 DTB_FILE_NAME=${PLATFORM_NAME}.dtb STM32MP_RAW_NAND=1 STM32MP_USB_PROGRAMMER=1 DEBUG=${DEBUG_BUILD}

	[ "$?" -eq "0" ] && echo " OK" || (echo " [ERROR] an unexpected error occurred building the TF-A binary" && exit 1)

	echo ""
	echo "- Build FIP binary for: ${PLATFORM_NAME}"
	## Build command
	make ARM_ARCH_MAJOR=7 ARCH=aarch32 PLAT=stm32mp1 DTB_FILE_NAME=${PLATFORM_NAME}.dtb AARCH32_SP=optee BL32=${OPTEE_DIR}/build/core/tee-header_v2.bin BL32_EXTRA1=${OPTEE_DIR}/build/core/tee-pager_v2.bin BL32_EXTRA2=${OPTEE_DIR}/build/core/tee-pageable_v2.bin BL33=${UBOOT_DIR}/u-boot-nodtb.bin BL33_CFG=${UBOOT_DIR}/u-boot.dtb DEBUG=${DEBUG_BUILD} fip

	[ "$?" -eq "0" ] && echo " OK" || (echo " [ERROR] an unexpected error occurred building the FIP binary" && exit 1)

	[ ${DEBUG_BUILD} -eq 0 ] && OUTPUT_BUILD_FOLDER="build/stm32mp1/release" || OUTPUT_BUILD_FOLDER="build/stm32mp1/debug"
	cp ${OUTPUT_BUILD_FOLDER}/fip.bin ${OUTPUT_PATH}/fip-${PLATFORM_NAME}-optee.bin && \
	cp ${OUTPUT_BUILD_FOLDER}/tf-a-${PLATFORM_NAME}.stm32 ${OUTPUT_PATH}/tf-a-${PLATFORM_NAME}-nand.stm32

	if [ "$?" -eq "0" ]; then
		echo " TF-A and FIP binaries generated successfully"
		echo "    - ${OUTPUT_PATH}/fip-${PLATFORM_NAME}-optee.bin"
		echo "    - ${OUTPUT_PATH}/tf-a-${PLATFORM_NAME}-nand.stm32"
	else
		echo " [ERROR] an unexpected error occurred while copying the TF-A/FIP binary"
		exit 1
	fi
}

clean_optee_repo()
{
	echo "- Remove clone OPTEE repository...."
	rm -rf ${OPTEE_DIR}
}

clean_atf_repo()
{
	echo "- Remove clone arm-trusted-firmware repository...."
	rm -rf ${ATF_DIR}
}

##### Main
WORKSPACE="${WORKSPACE:-$( cd "$( dirname "$0" )" && pwd )}"
DEBUG_BUILD="${DEBUG_BUILD:-0}"
CLEAN_BUILD="${CLEAN_BUILD:-0}"

OPTEE_REPO="${OPTEE_REPO:-https://github.com/digi-embedded/optee_os.git}"
OPTEE_REVISION="${OPTEE_REVISION:-3.16.0/stm/maint}"
OPTEE_DIR="${OPTEE_DIR:-${WORKSPACE}/optee_os}"
OPTEE_PATCHES=""

ATF_REPO="${ATF_REPO:-https://github.com/digi-embedded/arm-trusted-firmware.git}"
ATF_REVISION="${ATF_REVISION:-v2.6/stm32mp/maint}"
ATF_DIR="${ATF_DIR:-${WORKSPACE}/arm-trusted-firmware}"
ATF_PATCHES=""

UBOOT_DIR="${UBOOT_DIR:-${WORKSPACE}/images}"
OUTPUT_PATH="${OUTPUT_PATH:-${WORKSPACE}/output}"

# Parse command line arguments
while [ "${1}" != "" ]; do
	case ${1} in
		-c|--clean )		shift; CLEAN_BUILD=1;;	
		-d|--debug )		shift; DEBUG_BUILD=1;;
		-o|--output )		shift; OUTPUT_PATH=$1;;
		-p|--platform )		shift; PLATFORM_NAME=$1;;
		-t|--toolchain )	shift; TOOLCHAIN=$1;;
		-u|--uboot )		shift; UBOOT_DIR=$1;;
		-h|--help )		usage; exit;;
		*) 			echo "[ERROR] Unknown option"; usage; exit 1;;
	esac
	shift
done

# Define variables depending on the platform
if [ "${PLATFORM_NAME}" = "ccmp13-dvk-256MB" ]; then
	OPTEE_CFG_STM32_EARLY_CONSOLE_UART="5"
elif [ "${PLATFORM_NAME}" = "ccmp15-dvk-512MB" ] || [ "${PLATFORM_NAME}" = "ccmp15-dvk-1GB" ]; then
	OPTEE_CFG_STM32_EARLY_CONSOLE_UART="4"
else
	echo " ${PLATFORM_NAME} is not supported"
	exit 1
fi

# Start build process
validate_environment
clone_optee_repo
clone_atf_repo
build_optee
build_atf
if [ "$CLEAN_BUILD" -eq 1 ]; then
	clean_optee_repo
	clean_atf_repo
fi
