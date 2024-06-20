#!/bin/sh

usage() {
        cat <<EOF

Usage: $(basename "$0")

Generate bootloader for i.MX devices.

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

clone_atf_repo()
{
	if [ ! -d "${ATF_DIR}" ]; then
		echo "- Clone imx-atf repository:"
		git clone "${ATF_REPO}" -b "${ATF_BRANCH}" "${ATF_DIR}"
	fi

	(
		cd "${ATF_DIR}" || exit 1
		git clean -ffdx && git restore .
		echo "- Update imx-atf repository:"
		git pull "$(git remote)"
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
			patch -p1 < "${BASEDIR}"/patch/ccimx93/"${p}" || exit 2
		done
	)
}

build_atf()
{
	echo "- Build ATF binary for: ${SOC}"
	(
		cd "${ATF_DIR}" || { echo "build_atf: ATF_DIR not found"; exit 1; }
		${MAKE} CROSS_COMPILE=${CROSS_COMPILE} LD=${CROSS_COMPILE}ld.bfd CC=${CROSS_COMPILE}gcc PLAT="${ATF_PLAT}" realclean
		${MAKE} CROSS_COMPILE=${CROSS_COMPILE} LD=${CROSS_COMPILE}ld.bfd CC=${CROSS_COMPILE}gcc PLAT="${ATF_PLAT}" bl31
		${MAKE} CROSS_COMPILE=${CROSS_COMPILE} LD=${CROSS_COMPILE}ld.bfd CC=${CROSS_COMPILE}gcc PLAT="${ATF_PLAT}" BUILD_BASE=build-optee realclean
		${MAKE} CROSS_COMPILE=${CROSS_COMPILE} LD=${CROSS_COMPILE}ld.bfd CC=${CROSS_COMPILE}gcc PLAT="${ATF_PLAT}" BUILD_BASE=build-optee SPD=opteed bl31

		# Build ATF with workaround for SOC revision A0
		${MAKE} CROSS_COMPILE=${CROSS_COMPILE} LD=${CROSS_COMPILE}ld.bfd CC=${CROSS_COMPILE}gcc PLAT="${ATF_PLAT}" SOC_REV_A0=1 BUILD_BASE=build-A0 realclean
		${MAKE} CROSS_COMPILE=${CROSS_COMPILE} LD=${CROSS_COMPILE}ld.bfd CC=${CROSS_COMPILE}gcc PLAT="${ATF_PLAT}" SOC_REV_A0=1 BUILD_BASE=build-A0 bl31
		${MAKE} CROSS_COMPILE=${CROSS_COMPILE} LD=${CROSS_COMPILE}ld.bfd CC=${CROSS_COMPILE}gcc PLAT="${ATF_PLAT}" SOC_REV_A0=1 BUILD_BASE=build-A0-optee realclean
		${MAKE} CROSS_COMPILE=${CROSS_COMPILE} LD=${CROSS_COMPILE}ld.bfd CC=${CROSS_COMPILE}gcc PLAT="${ATF_PLAT}" SOC_REV_A0=1 BUILD_BASE=build-A0-optee SPD=opteed bl31
	)
}

clone_optee_repo()
{
	if [ ! -d "${OPTEE_DIR}" ]; then
		echo "- Clone imx-optee-os repository:"
		git clone "${OPTEE_REPO}" -b "${OPTEE_BRANCH}" "${OPTEE_DIR}"
	fi

	(
		cd "${OPTEE_DIR}" || { echo "build_atf: OPTEE_DIR not found"; exit 1; }
		git clean -ffdx && git restore .
		echo "- Update imx-optee-os repository:"
		git pull "$(git remote)"
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
			patch -p1 < "${BASEDIR}"/patch/ccimx93/"${p}" || exit 2
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
		${MAKE} PLATFORM=imx-ccimx93dvk \
			CROSS_COMPILE=${CROSS_COMPILE} \
			CROSS_COMPILE64=${CROSS_COMPILE} \
			CFG_TEE_TA_LOG_LEVEL=0 \
			CFG_TEE_CORE_LOG_LEVEL=0 \
			COMPILER=gcc \
			O=build

		# Build OPTEE for SOC revision A0
		${MAKE} PLATFORM=imx-ccimx93dvk_a0 \
			CROSS_COMPILE=${CROSS_COMPILE} \
			CROSS_COMPILE64=${CROSS_COMPILE} \
			CFG_TEE_TA_LOG_LEVEL=0 \
			CFG_TEE_CORE_LOG_LEVEL=0 \
			COMPILER=gcc \
			O=build-A0
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
		git pull "$(git remote)"
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
			patch -p1 < "${BASEDIR}"/patch/ccimx93/"${p}" || exit 2
		done
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
	[ -d "${FIRMWARE_ELE_DIR}" ] && { echo "- Sentinel firmware already downloaded"; return 0; }

	(
		cd "${BASEDIR}" || { echo "download_firmware_ele: BASEDIR not found"; exit 1; }
		if [ ! -f "${FIRMWARE_ELE}.bin" ]; then
			if ! wget "${FIRMWARE_ELE_URL}"; then
				echo "- Unable to download Sentinel firmware from ${FIRMWARE_ELE_URL}"
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
		cp --remove-destination "${UBOOT_DIR}"/u-boot.bin "${MKIMAGE_DIR}"/"${SOC}"
		cp --remove-destination "${UBOOT_DIR}"/spl/u-boot-spl.bin "${MKIMAGE_DIR}"/"${SOC}"
		cp --remove-destination "${UBOOT_DIR}"/tools/mkimage "${MKIMAGE_DIR}"/"${SOC}"/mkimage_uboot
	else
		echo "- Missing u-boot artifacts"
		exit 1
	fi

	# LPDDR4 training files
	(
		cd "${FIRMWARE_IMX_DIR}"/firmware/ddr/synopsys || exit 1
		cp --remove-destination lpddr4_dmem_1d_v202201.bin lpddr4_dmem_2d_v202201.bin \
			lpddr4_imem_1d_v202201.bin lpddr4_imem_2d_v202201.bin "${MKIMAGE_DIR}"/"${SOC}"
	)

	# AHAB container
	cp --remove-destination "${FIRMWARE_ELE_DIR}"/mx93??-ahab-container.img "${MKIMAGE_DIR}"/"${SOC}"

	# ATF
	cp --remove-destination "${ATF_DIR}"/build/"${ATF_PLAT}"/release/bl31.bin "${MKIMAGE_DIR}"/"${SOC}"/bl31-imx93.bin
	cp --remove-destination "${ATF_DIR}"/build-optee/"${ATF_PLAT}"/release/bl31.bin "${MKIMAGE_DIR}"/"${SOC}"/bl31-imx93.bin-optee
	cp --remove-destination "${ATF_DIR}"/build-A0/"${ATF_PLAT}"/release/bl31.bin "${MKIMAGE_DIR}"/"${SOC}"/bl31-imx93-A0.bin
	cp --remove-destination "${ATF_DIR}"/build-A0-optee/"${ATF_PLAT}"/release/bl31.bin "${MKIMAGE_DIR}"/"${SOC}"/bl31-imx93-A0.bin-optee

	# OPTEE binary
	cp --remove-destination "${OPTEE_DIR}"/build/core/tee-raw.bin "${MKIMAGE_DIR}"/"${SOC}"
	cp --remove-destination "${OPTEE_DIR}"/build-A0/core/tee-raw.bin "${MKIMAGE_DIR}"/"${SOC}"/tee-raw-A0.bin
}

build_imxboot()
{
	cd "${MKIMAGE_DIR}" || exit 1
	mkdir -p "${OUTPUT_PATH}"

	(
		for rev in A0 A1; do
			BL31_BIN="bl31-imx93.bin"
			TEE_BIN="tee-raw.bin"
			if [ "${rev}" = "A0" ]; then
				BL31_BIN="bl31-imx93-A0.bin"
				TEE_BIN="tee-raw-A0.bin"
			fi

			echo "- Build imx-boot (NO-OPTEE) binary for: ${SOC} (${rev})"
			${MAKE} SOC="${SOC}" REV="${rev}" clean
			rm -f "${SOC}"/tee.bin "${SOC}"/bl31.bin
			[ -f "${SOC}"/${BL31_BIN} ] && ln -sf ${BL31_BIN} "${SOC}"/bl31.bin
			${MAKE} SOC="${SOC}" REV="${rev}" flash_singleboot
			cp --remove-destination "${SOC}"/flash.bin "${OUTPUT_PATH}"/imx-boot-ccimx93-dvk-${rev}-nooptee.bin
			cp --remove-destination "${SOC}"/mkimage-flash_singleboot.log "${OUTPUT_PATH}"/mkimage-ccimx93-dvk-${rev}-nooptee-flash_singleboot.log

			echo "- Build imx-boot (OPTEE) binary for: ${SOC} (${rev})"
			${MAKE} SOC="${SOC}" REV="${rev}" clean
			rm -f "${SOC}"/tee.bin "${SOC}"/bl31.bin
			[ -f "${SOC}"/${BL31_BIN}-optee ] && ln -sf ${BL31_BIN}-optee "${SOC}"/bl31.bin
			[ -f "${SOC}"/${TEE_BIN} ] && ln -sf ${TEE_BIN} "${SOC}"/tee.bin
			${MAKE} SOC="${SOC}" REV="${rev}" flash_singleboot
			cp --remove-destination "${MKIMAGE_DIR}"/"${SOC}"/flash.bin "${OUTPUT_PATH}"/imx-boot-ccimx93-dvk-${rev}.bin
			cp --remove-destination "${SOC}"/mkimage-flash_singleboot.log "${OUTPUT_PATH}"/mkimage-ccimx93-dvk-${rev}-flash_singleboot.log
		done
	)
}

##### Main
BASEDIR="$(cd "$(dirname "$0")" && pwd)"

MKIMAGE_REPO="https://github.com/nxp-imx/imx-mkimage.git"
MKIMAGE_BRANCH="lf-6.1.55_2.2.0"
# Tag: lf-6.1.55-2.2.0
MKIMAGE_REV="c4365450fb115d87f245df2864fee1604d97c06a"
MKIMAGE_DIR="${BASEDIR}/imx-mkimage"
MKIMAGE_PATCHES=" \
	mkimage/0001-imx9-soc.mak-capture-commands-output-into-a-log-file.patch \
"

ATF_REPO="https://github.com/nxp-imx/imx-atf.git"
ATF_BRANCH="lf_v2.8"
# Tag: lf-6.1.55-2.2.0
ATF_REV="08e9d4eef2262c0dd072b4325e8919e06d349e02"
ATF_DIR="${BASEDIR}/imx-atf"
ATF_PATCHES=" \
	atf/0001-ccimx93-use-UART6-for-the-default-console.patch
	atf/0002-imx93-bring-back-ELE-clock-workaround-for-soc-revisi.patch
"

OPTEE_REPO="https://github.com/nxp-imx/imx-optee-os.git"
OPTEE_BRANCH="lf-6.1.55_2.2.0"
# Tag: lf-6.1.55-2.2.0
OPTEE_REV="a303fc80f7c4bd713315687a1fa1d6ed136e78ee"
OPTEE_DIR="${BASEDIR}/imx-optee-os"
OPTEE_PATCHES=" \
	optee/0007-allow-setting-sysroot-for-clang.patch \
	optee/0001-core-imx-support-ccimx93-dvk.patch \
	optee/0002-core-ccimx93-enable-AES_HUK-trusted-application.patch \
"

FIRMWARE_IMX="firmware-imx-8.22"
FIRMWARE_IMX_DIR="${BASEDIR}/${FIRMWARE_IMX}"
FIRMWARE_IMX_URL="https://www.nxp.com/lgfiles/NMG/MAD/YOCTO/${FIRMWARE_IMX}.bin"

FIRMWARE_ELE="firmware-ele-imx-0.1.0"
FIRMWARE_ELE_DIR="${BASEDIR}/${FIRMWARE_ELE}"
FIRMWARE_ELE_URL="https://www.nxp.com/lgfiles/NMG/MAD/YOCTO/${FIRMWARE_ELE}.bin"

SOC="iMX9"
ATF_PLAT="imx93"

OUTPUT_PATH="${BASEDIR}/output"
UBOOT_DIR="${UBOOT_DIR:-$(realpath "${BASEDIR}"/../..)}"

# Parse command line arguments
while [ "${1}" != "" ]; do
	case ${1} in
		-h|--help) usage; exit 0;;
		*) echo "[ERROR] Unknown option"; usage; exit 1;;
	esac
	shift
done

CPUS="$(echo /sys/devices/system/cpu/cpu[0-9]* | wc -w)"
[ "${CPUS}" -gt 1 ] && MAKE_JOBS="-j${CPUS}"
MAKE="make ${MAKE_JOBS}"

validate_environment
download_firmware_imx
download_firmware_ele
clone_atf_repo
patch_atf_repo
build_atf
clone_optee_repo
patch_optee_repo
build_optee
clone_mkimage_repo
patch_mkimage_repo
copy_artifacts_mkimage_folder
build_imxboot
