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
			patch -p1 < "${BASEDIR}"/patch/ccimx91/"${p}" || exit 2
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
			patch -p1 < "${BASEDIR}"/patch/ccimx91/"${p}" || exit 2
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
		${MAKE} PLATFORM=imx-ccimx91dvk \
			CROSS_COMPILE="${CROSS_COMPILE}" \
			CROSS_COMPILE64="${CROSS_COMPILE}" \
			CFG_TEE_TA_LOG_LEVEL=0 \
			CFG_TEE_CORE_LOG_LEVEL=0 \
			COMPILER=gcc \
			O=build
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
			patch -p1 < "${BASEDIR}"/patch/ccimx91/"${p}" || exit 2
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

	# LPDDR4 training files
	(
		cd "${FIRMWARE_IMX_DIR}"/firmware/ddr/synopsys || exit 1
		cp --remove-destination lpddr4_dmem_1d_v202201.bin lpddr4_dmem_2d_v202201.bin \
			lpddr4_imem_1d_v202201.bin lpddr4_imem_2d_v202201.bin "${MKIMAGE_SOC_DIR}"
	)

	# AHAB container, ATF and Optee binaries
	cp --remove-destination "${FIRMWARE_ELE_DIR}"/mx91a0-ahab-container.img "${MKIMAGE_SOC_DIR}"
	cp --remove-destination "${ATF_DIR}"/build/"${ATF_PLAT}"/release/bl31.bin "${MKIMAGE_SOC_DIR}"/bl31-imx91.bin
	cp --remove-destination "${ATF_DIR}"/build-optee/"${ATF_PLAT}"/release/bl31.bin "${MKIMAGE_SOC_DIR}"/bl31-imx91.bin-optee
	cp --remove-destination "${OPTEE_DIR}"/build/core/tee-raw.bin "${MKIMAGE_SOC_DIR}"
}

build_imxboot()
{
	cd "${MKIMAGE_DIR}" || exit 1
	mkdir -p "${OUTPUT_PATH}"

	(
		BL31_BIN="bl31-imx91.bin"
		TEE_BIN="tee-raw.bin"

		echo "- Build imx-boot (NO-OPTEE) binary for: ${SOC}"
		${MAKE} SOC="${SOC}" clean
		rm -f "${MKIMAGE_SOC_DIR}"/tee.bin "${MKIMAGE_SOC_DIR}"/bl31.bin
		[ -f "${MKIMAGE_SOC_DIR}"/${BL31_BIN} ] && ln -sf ${BL31_BIN} "${MKIMAGE_SOC_DIR}"/bl31.bin
		${MAKE} SOC="${SOC}" flash_singleboot
		cp --remove-destination "${MKIMAGE_SOC_DIR}"/flash.bin "${OUTPUT_PATH}"/imx-boot-ccimx91-dvk-nooptee.bin
		cp --remove-destination "${MKIMAGE_SOC_DIR}"/mkimage-flash_singleboot.log "${OUTPUT_PATH}"/mkimage-ccimx91-dvk-nooptee-flash_singleboot.log

		echo "- Build imx-boot (OPTEE) binary for: ${SOC}"
		${MAKE} SOC="${SOC}" clean
		rm -f "${MKIMAGE_SOC_DIR}"/tee.bin "${MKIMAGE_SOC_DIR}"/bl31.bin
		[ -f "${MKIMAGE_SOC_DIR}"/${BL31_BIN}-optee ] && ln -sf ${BL31_BIN}-optee "${MKIMAGE_SOC_DIR}"/bl31.bin
		[ -f "${MKIMAGE_SOC_DIR}"/${TEE_BIN} ] && ln -sf ${TEE_BIN} "${MKIMAGE_SOC_DIR}"/tee.bin
		${MAKE} SOC="${SOC}" flash_singleboot
		cp --remove-destination "${MKIMAGE_SOC_DIR}"/flash.bin "${OUTPUT_PATH}"/imx-boot-ccimx91-dvk.bin
		cp --remove-destination "${MKIMAGE_SOC_DIR}"/mkimage-flash_singleboot.log "${OUTPUT_PATH}"/mkimage-ccimx91-dvk-flash_singleboot.log
	)
}

##### Main
BASEDIR="$(cd "$(dirname "$0")" && pwd)"

MKIMAGE_REPO="https://github.com/nxp-imx/imx-mkimage.git"
MKIMAGE_BRANCH="lf-6.6.23_2.0.0"
# Tag: lf-6.6.23-2.0.0
MKIMAGE_REV="ca5d6b2d3fd9ab15825b97f7ef6f1ce9a8644966"
MKIMAGE_DIR="${BASEDIR}/imx-mkimage"
MKIMAGE_SOC_DIR="${MKIMAGE_DIR}/iMX91"
MKIMAGE_PATCHES=" \
	mkimage/0001-imx91-soc.mak-capture-commands-output-into-a-log-fil.patch \
"

ATF_REPO="https://github.com/nxp-imx/imx-atf.git"
ATF_BRANCH="lf_v2.10"
# Tag: lf-6.6.23-2.0.0
ATF_REV="49143a1701d9ccd3239e3f95f3042897ca889ea8"
ATF_DIR="${BASEDIR}/imx-atf"
ATF_PATCHES=" \
	atf/0001-ccimx91-use-UART6-for-the-default-console.patch \
"

OPTEE_REPO="https://github.com/nxp-imx/imx-optee-os.git"
OPTEE_BRANCH="lf-6.6.23_2.0.0"
# Tag: lf-6.6.23-2.0.0
OPTEE_REV="c6be5b572452a2808d1a34588fd10e71715e23cf"
OPTEE_DIR="${BASEDIR}/imx-optee-os"
OPTEE_PATCHES=" \
	optee/0001-core-imx-support-ccimx91-dvk.patch \
"

FIRMWARE_IMX="firmware-imx-8.24-fbe0a4c"
FIRMWARE_IMX_DIR="${BASEDIR}/${FIRMWARE_IMX}"
FIRMWARE_IMX_URL="https://www.nxp.com/lgfiles/NMG/MAD/YOCTO/${FIRMWARE_IMX}.bin"

FIRMWARE_ELE="firmware-ele-imx-0.1.2-4ed450a"
FIRMWARE_ELE_DIR="${BASEDIR}/${FIRMWARE_ELE}"
FIRMWARE_ELE_URL="https://www.nxp.com/lgfiles/NMG/MAD/YOCTO/${FIRMWARE_ELE}.bin"

SOC="iMX91"
ATF_PLAT="imx91"

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
