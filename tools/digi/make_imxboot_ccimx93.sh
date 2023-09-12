#!/bin/sh

usage() {
        cat <<EOF

Usage: $(basename "$0") [OPTIONS]

    -u,--uboot <u-boot directory>       u-boot build directory

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
			patch -p1 < "${WORKSPACE}"/patch/ccimx93/"${p}" || exit 2
		done
	)
}

build_atf()
{
	echo "- Build ATF binary for: ${SOC}"
	(
		cd "${ATF_DIR}" || { echo "build_atf: ATF_DIR not found"; exit 1; }
		${MAKE} CROSS_COMPILE=${CROSS_COMPILE} LD=${CROSS_COMPILE}ld CC=${CROSS_COMPILE}gcc PLAT="${ATF_PLAT}" realclean
		${MAKE} CROSS_COMPILE=${CROSS_COMPILE} LD=${CROSS_COMPILE}ld CC=${CROSS_COMPILE}gcc PLAT="${ATF_PLAT}" bl31
		${MAKE} CROSS_COMPILE=${CROSS_COMPILE} LD=${CROSS_COMPILE}ld CC=${CROSS_COMPILE}gcc PLAT="${ATF_PLAT}" BUILD_BASE=build-optee realclean
		${MAKE} CROSS_COMPILE=${CROSS_COMPILE} LD=${CROSS_COMPILE}ld CC=${CROSS_COMPILE}gcc PLAT="${ATF_PLAT}" BUILD_BASE=build-optee SPD=opteed bl31
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
			patch -p1 < "${WORKSPACE}"/patch/ccimx93/"${p}" || exit 2
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
			patch -p1 < "${WORKSPACE}"/patch/ccimx93/"${p}" || exit 2
		done
	)
}

download_firmware_imx()
{
	[ -d "${FIRMWARE_IMX_DIR}" ] && { echo "- IMX firmware already downloaded"; return 0; }

	(
		cd "${WORKSPACE}" || { echo "download_firmware_imx: WORKSPACE not found"; exit 1; }
		if [ ! -f "${FIRMWARE_IMX}.bin" ]; then
			if ! wget "${FIRMWARE_IMX_URL}"; then
				echo "- Unable to download IMX firmware from ${FIRMWARE_IMX_URL}"
				exit 1
			fi
		fi
		sh "${FIRMWARE_IMX}.bin" --auto-accept --force
	)
}

download_firmware_sentinel()
{
	[ -d "${FIRMWARE_SENTINEL_DIR}" ] && { echo "- Sentinel firmware already downloaded"; return 0; }

	(
		cd "${WORKSPACE}" || { echo "download_firmware_sentinel: WORKSPACE not found"; exit 1; }
		if [ ! -f "${FIRMWARE_SENTINEL}.bin" ]; then
			if ! wget "${FIRMWARE_SENTINEL_URL}"; then
				echo "- Unable to download Sentinel firmware from ${FIRMWARE_SENTINEL_URL}"
				exit 1
			fi
		fi
		sh "${FIRMWARE_SENTINEL}.bin" --auto-accept --force
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
	cp --remove-destination "${FIRMWARE_SENTINEL_DIR}"/mx93a0-ahab-container.img "${MKIMAGE_DIR}"/"${SOC}"

	# ATF
	cp --remove-destination "${ATF_DIR}"/build/"${ATF_PLAT}"/release/bl31.bin "${MKIMAGE_DIR}"/"${SOC}"/bl31-imx93.bin
	cp --remove-destination "${ATF_DIR}"/build-optee/"${ATF_PLAT}"/release/bl31.bin "${MKIMAGE_DIR}"/"${SOC}"/bl31-imx93.bin-optee

	# OPTEE binary
	cp --remove-destination "${OPTEE_DIR}"/build/core/tee-raw.bin "${MKIMAGE_DIR}"/"${SOC}"
}

build_imxboot()
{
	cd "${MKIMAGE_DIR}" || exit 1
	mkdir -p "${OUTPUT_PATH}"

	(
		echo "- Build imx-boot binary for: ${SOC}"
		${MAKE} SOC="${SOC}" clean
		[ -f "${SOC}"/bl31-imx93.bin ] && ln -sf bl31-imx93.bin "${SOC}"/bl31.bin
		rm -f "${SOC}"/tee.bin
		${MAKE} SOC="${SOC}" flash_singleboot
		cp --remove-destination "${SOC}"/flash.bin "${OUTPUT_PATH}"/imx-boot-ccimx93-dvk.bin
		cp --remove-destination "${SOC}"/mkimage-flash_singleboot.log "${OUTPUT_PATH}"/mkimage-flash_singleboot.log

		echo "- Build imx-boot (OPTEE) binary for: ${SOC}"
		${MAKE} SOC="${SOC}" clean
		[ -f "${SOC}"/bl31-imx93.bin-optee ] && ln -sf bl31-imx93.bin-optee "${SOC}"/bl31.bin
		[ -f "${SOC}"/tee-raw.bin ] && ln -sf tee-raw.bin "${SOC}"/tee.bin
		${MAKE} SOC="${SOC}" flash_singleboot
		cp --remove-destination "${MKIMAGE_DIR}"/"${SOC}"/flash.bin "${OUTPUT_PATH}"/imx-boot-ccimx93-dvk.bin-optee
		cp --remove-destination "${SOC}"/mkimage-flash_singleboot.log "${OUTPUT_PATH}"/mkimage-flash_singleboot.log-optee
	)
}

##### Main
WORKSPACE="$(cd "$(dirname "$0")" && pwd)"

MKIMAGE_REPO="https://github.com/nxp-imx/imx-mkimage.git"
MKIMAGE_BRANCH="lf-6.1.22_2.0.0"
# Tag: lf-6.1.22-2.0.0
MKIMAGE_REV="5cfd218012e080fb907d9cc301fbb4ece9bc17a9"
MKIMAGE_DIR="${WORKSPACE}/imx-mkimage"
MKIMAGE_PATCHES=" \
	mkimage/0001-imx9-soc.mak-capture-commands-output-into-a-log-file.patch \
"

ATF_REPO="https://github.com/nxp-imx/imx-atf.git"
ATF_BRANCH="lf_v2.8"
# Tag: lf-6.1.22-2.0.0
ATF_REV="99195a23d3aef485fb8f10939583b1bdef18881c"
ATF_DIR="${WORKSPACE}/imx-atf"
ATF_PATCHES=" \
	atf/0001-imx8mm-Define-UART1-as-console-for-boot-stage.patch \
	atf/0002-imx8mm-Disable-M4-debug-console.patch \
	atf/0003-imx8mn-Define-UART1-as-console-for-boot-stage.patch \
	atf/0004-imx8mn-Disable-M7-debug-console.patch \
	atf/0005-ccimx93-use-UART6-for-the-default-console.patch \
"

OPTEE_REPO="https://github.com/nxp-imx/imx-optee-os.git"
OPTEE_BRANCH="lf-6.1.22_2.0.0"
# Tag: lf-6.1.22-2.0.0
OPTEE_REV="1962aec9581760803b1485d455cd62cb11c14870"
OPTEE_DIR="${WORKSPACE}/imx-optee-os"
OPTEE_PATCHES=" \
	optee/0007-allow-setting-sysroot-for-clang.patch \
	optee/0001-core-imx-support-ccimx93-dvk.patch \
"

FIRMWARE_IMX="firmware-imx-8.20"
FIRMWARE_IMX_DIR="${WORKSPACE}/${FIRMWARE_IMX}"
FIRMWARE_IMX_URL="https://www.nxp.com/lgfiles/NMG/MAD/YOCTO/${FIRMWARE_IMX}.bin"

FIRMWARE_SENTINEL="firmware-sentinel-0.10"
FIRMWARE_SENTINEL_DIR="${WORKSPACE}/${FIRMWARE_SENTINEL}"
FIRMWARE_SENTINEL_URL="https://www.nxp.com/lgfiles/NMG/MAD/YOCTO/${FIRMWARE_SENTINEL}.bin"

SOC="iMX9"
ATF_PLAT="imx93"

OUTPUT_PATH="${WORKSPACE}/output"

# Parse command line arguments
while [ "${1}" != "" ]; do
	case ${1} in
		-u|--uboot) shift; UBOOT_DIR="${1}";;
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
download_firmware_sentinel
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
