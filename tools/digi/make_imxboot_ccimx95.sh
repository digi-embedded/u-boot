#!/bin/sh

set -e

usage() {
        cat <<EOF

Usage: $(basename "$0")

Generate bootloader for i.MX devices.

EOF
}

#
# Check cross-compilers existence
#
# Toolchain (bare-metal) for Cortex-M binaries (SM and OEI)
# https://developer.arm.com/-/media/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-x86_64-arm-none-eabi.tar.xz
#
validate_environment()
{
	if [ -n "${CROSS_COMPILE}" ] && [ -n "${CORTEX_M_CROSS_COMPILE}" ]; then
		if type "${CROSS_COMPILE}"gcc >/dev/null; then
			type "${CORTEX_M_CROSS_COMPILE}"gcc >/dev/null && return
		fi
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

	# Check Cortex-M cross-compiler
	CORTEX_M_CROSS_COMPILE="arm-none-eabi-"
	if ! type ${CORTEX_M_CROSS_COMPILE}gcc >/dev/null; then
		echo "[ERROR] Missing Cortex-M cross-compiler"
		exit 1
	fi
}

clone_atf_repo()
{
	if [ ! -d "${ATF_DIR}" ]; then
		echo "- Clone imx-atf repository:"
		git clone "${ATF_REPO}" -b "${ATF_BRANCH}" "${ATF_DIR}"
	fi

	(
		cd "${ATF_DIR}" || { echo "clone_atf_repo: ATF_DIR not found"; exit 1; }
		git clean -ffdx && git restore .
		echo "- Update imx-atf repository:"
		git fetch "$(git remote)"
		git -c core.fsync=loose-object -c gc.autoDetach=false -c core.pager=cat checkout -B "${ATF_BRANCH}" "${ATF_REV}"
	)
}

patch_atf_repo()
{
	if [ ! -d "${ATF_DIR}" ]; then
		echo "- Missing imx-atf repository"
		exit 1
	fi

	echo "- Patch imx-atf repository:"
	(
		cd "${ATF_DIR}" || exit 1
		for p in ${ATF_PATCHES}; do
			echo "- Apply patch: ${p}"
			patch -p1 < "${BASEDIR}"/patch/atf/"${p}" || exit 2
		done
	)
}

build_atf()
{
	echo "- Build ATF binary for: ${SOC}"
	(
		cd "${ATF_DIR}" || { echo "build_atf: ATF_DIR not found"; exit 1; }
		${MAKE} CROSS_COMPILE="${CROSS_COMPILE}" LD="${CROSS_COMPILE}ld.bfd" CC="${CROSS_COMPILE}gcc" PLAT="${ATF_PLAT}" realclean
		${MAKE} CROSS_COMPILE="${CROSS_COMPILE}" LD="${CROSS_COMPILE}ld.bfd" CC="${CROSS_COMPILE}gcc" PLAT="${ATF_PLAT}" bl31
		${MAKE} CROSS_COMPILE="${CROSS_COMPILE}" LD="${CROSS_COMPILE}ld.bfd" CC="${CROSS_COMPILE}gcc" PLAT="${ATF_PLAT}" BUILD_BASE=build-optee realclean
		${MAKE} CROSS_COMPILE="${CROSS_COMPILE}" LD="${CROSS_COMPILE}ld.bfd" CC="${CROSS_COMPILE}gcc" PLAT="${ATF_PLAT}" BUILD_BASE=build-optee SPD=opteed bl31
	)
}

clone_optee_repo()
{
	if [ ! -d "${OPTEE_DIR}" ]; then
		echo "- Clone imx-optee-os repository:"
		git clone "${OPTEE_REPO}" -b "${OPTEE_BRANCH}" "${OPTEE_DIR}"
	fi

	(
		cd "${OPTEE_DIR}" || { echo "clone_optee_repo: OPTEE_DIR not found"; exit 1; }
		git clean -ffdx && git restore .
		echo "- Update imx-optee-os repository:"
		git fetch "$(git remote)"
		git -c core.fsync=loose-object -c gc.autoDetach=false -c core.pager=cat checkout -B "${OPTEE_BRANCH}" "${OPTEE_REV}"
	)
}

patch_optee_repo()
{
	if [ ! -d "${OPTEE_DIR}" ]; then
		echo "- Missing imx-optee-os repository"
		exit 1
	fi

	echo "- Patch imx-optee-os repository:"
	(
		cd "${OPTEE_DIR}" || exit 1
		for p in ${OPTEE_PATCHES}; do
			echo "- Apply patch: ${p}"
			patch -p1 < "${BASEDIR}"/patch/optee/"${p}" || exit 2
		done
	)
}

#
# Host packages required:
#   apt install python3-cryptography python3-pyelftools
#
build_optee()
{
	echo "- Build OPTEE binary for: ${SOC}"
	(
		cd "${OPTEE_DIR}" || { echo "build_optee: OPTEE_DIR not found"; exit 1; }
		${MAKE} PLATFORM=imx-ccimx95dvk \
			CROSS_COMPILE="${CROSS_COMPILE}" \
			CROSS_COMPILE64="${CROSS_COMPILE}" \
			CFG_TEE_TA_LOG_LEVEL=0 \
			CFG_TEE_CORE_LOG_LEVEL=0 \
			COMPILER=gcc \
			O=build
	)
}

clone_oei_repo()
{
	if [ ! -d "${OEI_DIR}" ]; then
		echo "- Clone Optional Executable Image (OEI) repository:"
		git clone "${OEI_REPO}" -b "${OEI_BRANCH}" "${OEI_DIR}"
	fi

	(
		cd "${OEI_DIR}" || { echo "clone_oei_repo: OEI_DIR not found"; exit 1; }
		git clean -ffdx && git restore .
		echo "- Update Optional Executable Image (OEI) repository:"
		git fetch "$(git remote)"
		git -c core.fsync=loose-object -c gc.autoDetach=false -c core.pager=cat checkout -B "${OEI_BRANCH}" "${OEI_REV}"
	)
}

patch_oei_repo()
{
	if [ ! -d "${OEI_DIR}" ]; then
		echo "- Missing imx-oei repository"
		exit 1
	fi

	echo "- Patch imx-oei repository:"
	(
		cd "${OEI_DIR}" || exit 1
		for p in ${OEI_PATCHES}; do
			echo "- Apply patch: ${p}"
			patch -p1 < "${BASEDIR}"/patch/oei/"${p}" || exit 2
		done
	)
}

build_oei()
{
	OEI_SOC_REV="${SOC_REV}"
	if [ "${OEI_SOC_REV#A}" != "${OEI_SOC_REV}" ]; then
		OEI_SOC_REV="A0"
		OEI_MAKE_ARGS="DDR_CONFIG=lpddr5_timing_a1"
	fi
	echo "- Build OEI binary for: ${OEI_BOARD}"
	(
		cd "${OEI_DIR}" || { echo "build_oei: OEI_DIR not found"; exit 1; }
		for oei_config in ddr tcm; do
			# shellcheck disable=SC2086 # allow splitting OEI_MAKE_ARGS
			${MAKE} board="${OEI_BOARD}" d=1 OEI_CROSS_COMPILE="${CORTEX_M_CROSS_COMPILE}" oei="${oei_config}" r="${OEI_SOC_REV}" ${OEI_MAKE_ARGS} clean
			# shellcheck disable=SC2086 # allow splitting OEI_MAKE_ARGS
			${MAKE} board="${OEI_BOARD}" d=1 OEI_CROSS_COMPILE="${CORTEX_M_CROSS_COMPILE}" oei="${oei_config}" r="${OEI_SOC_REV}" ${OEI_MAKE_ARGS}
		done
	)
	unset OEI_SOC_REV OEI_MAKE_ARGS
}

clone_sm_repo()
{
	if [ ! -d "${SM_DIR}" ]; then
		echo "- Clone system manager repository:"
		git clone "${SM_REPO}" -b "${SM_BRANCH}" "${SM_DIR}"
	fi

	(
		cd "${SM_DIR}" || { echo "clone_sm_repo: SM_DIR not found"; exit 1; }
		git clean -ffdx && git restore .
		echo "- Update system manager repository:"
		git fetch "$(git remote)"
		git -c core.fsync=loose-object -c gc.autoDetach=false -c core.pager=cat checkout -B "${SM_BRANCH}" "${SM_REV}"
	)
}

patch_sm_repo()
{
	if [ ! -d "${SM_DIR}" ]; then
		echo "- Missing imx-sm repository"
		exit 1
	fi

	echo "- Patch imx-sm repository:"
	(
		cd "${SM_DIR}" || exit 1
		for p in ${SM_PATCHES}; do
			echo "- Apply patch: ${p}"
			patch -p1 < "${BASEDIR}"/patch/sm/"${p}" || exit 2
		done
	)
}

build_sm()
{
	SM_M="0"
	echo "- Build SM binary for: ${SM_PLAT}"
	(
		cd "${SM_DIR}" || { echo "build_sm: SM_DIR not found"; exit 1; }
		${MAKE} CONFIG="${SM_PLAT}" SM_CROSS_COMPILE="${CORTEX_M_CROSS_COMPILE}" M="${SM_M}" clean
		${MAKE} CONFIG="${SM_PLAT}" SM_CROSS_COMPILE="${CORTEX_M_CROSS_COMPILE}" M="${SM_M}"
	)
}

clone_mkimage_repo()
{
	if [ ! -d "${MKIMAGE_DIR}" ]; then
		echo "- Clone imx-mkimage repository:"
		git clone "${MKIMAGE_REPO}" -b "${MKIMAGE_BRANCH}" "${MKIMAGE_DIR}"
	fi

	(
		cd "${MKIMAGE_DIR}" || { echo "clone_mkimage_repo: MKIMAGE_DIR not found"; exit 1; }
		git clean -ffdx && git restore .
		echo "- Update imx-mkimage repository:"
		git fetch "$(git remote)"
		git -c core.fsync=loose-object -c gc.autoDetach=false -c core.pager=cat checkout -B "${MKIMAGE_BRANCH}" "${MKIMAGE_REV}"
	)
}

patch_mkimage_repo()
{
	if [ ! -d "${MKIMAGE_DIR}" ]; then
		echo "- Missing imx-mkimage repository"
		exit 1
	fi

	echo "- Patch imx-mkimage repository:"
	(
		cd "${MKIMAGE_DIR}" || exit 1
		for p in ${MKIMAGE_PATCHES}; do
			echo "- Apply patch: ${p}"
			patch -p1 < "${BASEDIR}"/patch/mkimage/"${p}" || exit 2
		done
	)
}

download_firmware_m7()
{
	[ -d "${FIRMWARE_M7_DIR}" ] && { echo "- M7 firmware already downloaded"; return 0; }

	(
		cd "${BASEDIR}" || { echo "download_firmware_m7: BASEDIR not found"; exit 1; }
		if [ ! -f "${FIRMWARE_M7}.bin" ]; then
			if ! wget "${FIRMWARE_M7_URL}"; then
				echo "- Unable to download M7 firmware from ${FIRMWARE_M7_URL}"
				exit 1
			fi
		fi
		sh "${FIRMWARE_M7}.bin" --auto-accept --force
	)
}

download_firmware_imx()
{
	[ -d "${FIRMWARE_IMX_DIR}" ] && { echo "- IMX firmware already downloaded"; return 0; }

	(
		cd "${BASEDIR}" || { echo "download_firmware_imx: BASEDIR not found"; exit 1; }
		if [ ! -f "${FIRMWARE_IMX}.bin" ]; then
			if ! wget "${FIRMWARE_IMX_URL}"; then
				echo "- Unable to download IMX firmware from ${FIRMWARE_IMX_URL}"
				exit 1
			fi
		fi
		sh "${FIRMWARE_IMX}.bin" --auto-accept --force
	)
}

download_firmware_ele()
{
	[ -d "${FIRMWARE_ELE_DIR}" ] && { echo "- ELE firmware already downloaded"; return 0; }

	(
		cd "${BASEDIR}" || { echo "download_firmware_ele: BASEDIR not found"; exit 1; }
		if [ ! -f "${FIRMWARE_ELE}.bin" ]; then
			if ! wget "${FIRMWARE_ELE_URL}"; then
				echo "- Unable to download ELE firmware from ${FIRMWARE_ELE_URL}"
				exit 1
			fi
		fi
		sh "${FIRMWARE_ELE}.bin" --auto-accept --force
	)
}

copy_artifacts_mkimage_folder()
{
	( cd "${MKIMAGE_DIR}" && git clean -ffdx )
	echo "- Copy U-Boot binaries from ${UBOOT_DIR}"
	if [ ! -d "${UBOOT_DIR}" ]; then
		echo "- Missing u-boot directory: ${UBOOT_DIR}"
		exit 1
	elif [ -f "${UBOOT_DIR}/u-boot.bin" ] && [ -f "${UBOOT_DIR}/spl/u-boot-spl.bin" ] && [ -f "${UBOOT_DIR}/tools/mkimage" ]; then
		cp --remove-destination "${UBOOT_DIR}"/u-boot.bin "${MKIMAGE_SOC_DIR}"
		cp --remove-destination "${UBOOT_DIR}"/spl/u-boot-spl.bin "${MKIMAGE_SOC_DIR}"
		cp --remove-destination "${UBOOT_DIR}"/tools/mkimage "${MKIMAGE_SOC_DIR}"/mkimage_uboot
	else
		echo "- Missing u-boot artifacts"
		exit 1
	fi

	echo "- Copy rest of input artifacts: LPDDR fw, M7 fw, SM, OEI, ATF and Optee"
	# LPDDR4 training files
	(
		cd "${FIRMWARE_IMX_DIR}"/firmware/ddr/synopsys || exit 1
		cp --remove-destination lpddr5_dmem_qb_v202409.bin lpddr5_dmem_v202409.bin lpddr5_imem_qb_v202409.bin lpddr5_imem_v202409.bin "${MKIMAGE_SOC_DIR}"
	)

	# Cortex-M7 firmware
	cp --remove-destination "${FIRMWARE_M7_DIR}"/imx95-19x19-evk_m7_TCM_power_mode_switch.bin "${MKIMAGE_SOC_DIR}"/m7_image.bin

	# AHAB container, OEI, SM, ATF and Optee binaries
	cp --remove-destination "${FIRMWARE_ELE_DIR}"/mx95??-ahab-container.img "${MKIMAGE_SOC_DIR}"
	cp --remove-destination "${OEI_DIR}"/build/"${OEI_BOARD}"/*/oei-m33-*.bin "${MKIMAGE_SOC_DIR}"
	cp --remove-destination "${SM_DIR}"/build/"${SM_PLAT}"/m33_image.bin "${MKIMAGE_SOC_DIR}"
	cp --remove-destination "${ATF_DIR}"/build/"${ATF_PLAT}"/release/bl31.bin "${MKIMAGE_SOC_DIR}"/bl31-imx95.bin
	cp --remove-destination "${ATF_DIR}"/build-optee/"${ATF_PLAT}"/release/bl31.bin "${MKIMAGE_SOC_DIR}"/bl31-imx95.bin-optee
	cp --remove-destination "${OPTEE_DIR}"/build/core/tee-raw.bin "${MKIMAGE_SOC_DIR}"
}

#
# Yocto generated imx95 mkimage targets: flash_all flash_a55
#
# flash_all = flash_a55 + M7_IMG
#
build_imxboot()
{
	cd "${MKIMAGE_DIR}" || exit 1
	mkdir -p "${OUTPUT_PATH}"

	(
		BL31_BIN="bl31-imx95.bin"
		TEE_BIN="tee-raw.bin"

		echo "- Build imx-boot (NO-OPTEE) binary for: ${SOC}"
		${MAKE} SOC="${SOC}" clean
		rm -f "${MKIMAGE_SOC_DIR}"/tee.bin "${MKIMAGE_SOC_DIR}"/bl31.bin
		[ -f "${MKIMAGE_SOC_DIR}"/${BL31_BIN} ] && ln -sf ${BL31_BIN} "${MKIMAGE_SOC_DIR}"/bl31.bin
		${MAKE} SOC="${SOC}" REV="${SOC_REV}" OEI=YES LPDDR_TYPE=lpddr5 flash_all
		cp --remove-destination "${MKIMAGE_SOC_DIR}"/flash.bin "${OUTPUT_PATH}"/imx-boot-ccimx95-dvk-nooptee.bin
		cp --remove-destination "${MKIMAGE_SOC_DIR}"/mkimage-flash_all.log "${OUTPUT_PATH}"/mkimage-ccimx95-dvk-nooptee-flash_all.log

		echo "- Build imx-boot (OPTEE) binary for: ${SOC}"
		${MAKE} SOC="${SOC}" clean
		rm -f "${MKIMAGE_SOC_DIR}"/tee.bin "${MKIMAGE_SOC_DIR}"/bl31.bin
		[ -f "${MKIMAGE_SOC_DIR}"/${BL31_BIN}-optee ] && ln -sf ${BL31_BIN}-optee "${MKIMAGE_SOC_DIR}"/bl31.bin
		[ -f "${MKIMAGE_SOC_DIR}"/${TEE_BIN} ] && ln -sf ${TEE_BIN} "${MKIMAGE_SOC_DIR}"/tee.bin
		${MAKE} SOC="${SOC}" REV="${SOC_REV}" OEI=YES LPDDR_TYPE=lpddr5 flash_all
		cp --remove-destination "${MKIMAGE_SOC_DIR}"/flash.bin "${OUTPUT_PATH}"/imx-boot-ccimx95-dvk.bin
		cp --remove-destination "${MKIMAGE_SOC_DIR}"/mkimage-flash_all.log "${OUTPUT_PATH}"/mkimage-ccimx95-dvk-flash_all.log
	)
}

##### Main
BASEDIR="$(cd "$(dirname "$0")" && pwd)"

MKIMAGE_REPO="https://github.com/nxp-imx/imx-mkimage.git"
MKIMAGE_BRANCH="lf-6.6.52_2.2.1"
# Tag: lf-6.6.52-2.2.1
MKIMAGE_REV="81fca6434be0610f3f9216a762aadc4dc3e8d8db"
MKIMAGE_DIR="${BASEDIR}/imx-mkimage"
MKIMAGE_SOC_DIR="${MKIMAGE_DIR}/iMX95"
MKIMAGE_PATCHES=" \
	0001-iMX8QX-soc.mak-capture-commands-output-into-a-log-fi.patch \
	0002-imx8m-soc.mak-capture-commands-output-into-a-log-fil.patch \
	0003-imx8m-print_fit_hab-follow-symlinks.patch \
	0004-imx8mm-adjust-TEE_LOAD_ADDR-for-ccimx8mm.patch \
	0005-imx93-soc.mak-capture-commands-output-into-a-log-fil.patch \
	0006-imx93-soc.mak-add-makefile-target-to-build-A0-revisi.patch \
	0007-imx91-soc.mak-capture-commands-output-into-a-log-fil.patch \
	0008-imx95-soc.mak-capture-commands-output-into-a-log-fil.patch \
"

ATF_REPO="https://github.com/nxp-imx/imx-atf.git"
ATF_BRANCH="lf_v2.10_6.6.52_2.2.x"
# Tag: lf-6.6.52-2.2.1
ATF_REV="7e374c5f57328949a2b141a567175b6a2939e964"
ATF_DIR="${BASEDIR}/imx-atf"
ATF_PATCHES=" \
	0001-imx8mm-Define-UART1-as-console-for-boot-stage.patch \
	0002-imx8mm-Disable-M4-debug-console.patch \
	0003-imx8mn-Define-UART1-as-console-for-boot-stage.patch \
	0004-imx8mn-Disable-M7-debug-console.patch \
	0005-imx8mm-set-BL32_BASE-and-map-high-DRAM-for-ccimx8mm-.patch \
	0006-ccimx93-use-UART6-for-the-default-console.patch \
	0007-imx93-bring-back-ELE-clock-workaround-for-soc-revisi.patch \
	0008-ccimx91-use-UART6-for-the-default-console.patch \
	0009-ccimx95-set-DVK-console-to-LPUART6.patch \
	0010-ccimx95-enable-non-secure-non-privilege-access-to-GP.patch \
"

OPTEE_REPO="https://github.com/nxp-imx/imx-optee-os.git"
OPTEE_BRANCH="lf-6.6.52_2.2.0"
# Tag: lf-6.6.52-2.2.1
OPTEE_REV="ecea75b7fee5a3c8a2d9b99769ba78c4390c0e8b"
OPTEE_DIR="${BASEDIR}/imx-optee-os"
OPTEE_PATCHES=" \
	0001-plat-imx-add-support-for-ConnectCore-8M-Mini.patch \
	0002-core-imx-support-ccimx91-dvk.patch \
	0003-core-imx-support-ccimx93-dvk.patch \
	0004-core-ccimx93-enable-AES_HUK-trusted-application.patch \
	0005-core-imx-support-ccimx95-dvk.patch \
"

# Optional Executable Image running on the Cortex M33
OEI_REPO="https://github.com/nxp-imx/imx-oei.git"
OEI_BRANCH="master"
# Tag: lf-6.6.52-2.2.1
OEI_REV="ca91ce798b2f3a2a0bab8c0f835f4bea88c9b080"
OEI_DIR="${BASEDIR}/imx-oei"
OEI_PATCHES=" \
	0001-boards-ccimx95-add-platform-as-a-clone-of-mx95lp5.patch \
	0002-ddr-add-DDR-configuration-file-for-ccimx95.patch \
	0003-ccimx95-configure-console-on-LPUART6.patch \
	0004-ccimx95-add-DDR-configuration-file-for-ccimx95-B0-si.patch \
"

# System Manager running on the Cortex M33
SM_REPO="https://github.com/nxp-imx/imx-sm.git"
SM_BRANCH="master"
# Tag: lf-6.6.52-2.2.1
SM_REV="707569f402147029feb7f9b90811a6d6ea730bb6"
SM_DIR="${BASEDIR}/imx-sm"
SM_PATCHES=" \
	0001-ccimx95dvk-add-new-platform-config-and-board.patch \
	0002-ccimx95dvk-configure-board-and-switch-debug-UART-to-.patch \
	0003-ccimx95dvk-disable-PCAL6408A-expander-and-move-GPIO1.patch \
	0004-ccimx95dvk-move-resources-from-M7-to-A55.patch \
	0005-ccimx95dvk-move-pads-to-non-secure-A55.patch \
	0006-ccimx95dvk-move-CAN1-to-be-used-by-A55.patch \
	0007-ccimx95dvk-remove-PCAL6408A-IO-expander-from-EVK.patch \
	0008-ccimx95dvk-remove-PCA2123-RTC-from-EVK.patch \
	0009-ccimx95-change-names-of-voltage-regulators.patch  \
	0010-ccimx95dvk-enable-full-access-to-certain-regulators-.patch \
	0011-components-pf09-reduce-LDOs-step-to-50mV.patch \
	0012-ccimx95dvk-remove-access-to-VDD_3V3-and-VDD_1V8-from.patch \
"

FIRMWARE_IMX="firmware-imx-8.26.1-410be01"
FIRMWARE_IMX_DIR="${BASEDIR}/${FIRMWARE_IMX}"
FIRMWARE_IMX_URL="https://www.nxp.com/lgfiles/NMG/MAD/YOCTO/${FIRMWARE_IMX}.bin"

FIRMWARE_M7="imx95-m7-demo-25.06.00"
FIRMWARE_M7_DIR="${BASEDIR}/${FIRMWARE_M7}"
FIRMWARE_M7_URL="https://www.nxp.com/lgfiles/NMG/MAD/YOCTO/${FIRMWARE_M7}.bin"

FIRMWARE_ELE="firmware-ele-imx-2.0.2.1-d30b14a"
FIRMWARE_ELE_DIR="${BASEDIR}/${FIRMWARE_ELE}"
FIRMWARE_ELE_URL="https://www.nxp.com/lgfiles/NMG/MAD/YOCTO/${FIRMWARE_ELE}.bin"

SOC="iMX95"
SOC_REV="B0"
ATF_PLAT="imx95"
OEI_BOARD="ccimx95"
SM_PLAT="ccimx95dvk"

OUTPUT_PATH="${BASEDIR}/output"
UBOOT_DIR="${UBOOT_DIR:-$(realpath "${BASEDIR}"/../..)}"

# Parse command line arguments
while [ "${1}" != "" ]; do
	case ${1} in
		-h|--help) usage; exit 0;;
		*) echo "[ERROR] Unknown option"; usage; exit 1;;
	esac
done

CPUS="$(echo /sys/devices/system/cpu/cpu[0-9]* | wc -w)"
[ "${CPUS}" -gt 1 ] && MAKE_JOBS="-j${CPUS}"
MAKE="make ${MAKE_JOBS}"

validate_environment
download_firmware_imx
download_firmware_m7
download_firmware_ele
clone_atf_repo
patch_atf_repo
build_atf
clone_optee_repo
patch_optee_repo
build_optee
clone_oei_repo
patch_oei_repo
build_oei
clone_sm_repo
patch_sm_repo
build_sm
clone_mkimage_repo
patch_mkimage_repo
copy_artifacts_mkimage_folder
build_imxboot
