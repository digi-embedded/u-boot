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
			patch -p1 < "${BASEDIR}"/patch/ccimx8x/"${p}" || exit 2
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
			patch -p1 < "${BASEDIR}"/patch/ccimx8x/"${p}" || exit 2
		done
	)
}

download_digi_sc_firmware()
{
	[ -d "${DIGI_SC_FW_DIR}" ] && { echo "- Digi SC firmware already downloaded"; return 0; }

	(
		cd "${BASEDIR}" || { echo "download_digi_sc_firmware: BASEDIR not found"; exit 1; }
		if [ ! -f "${DIGI_SC_FW}.tar.gz" ]; then
			if ! wget "${DIGI_SC_FW_URL}"; then
				echo "- Unable to download Digi SC firmware from ${DIGI_SC_FW_URL}"
				exit 1
			fi
		fi
		tar xf "${DIGI_SC_FW}.tar.gz"
	)
}

download_firmware_seco()
{
	[ -d "${IMX_SECO_DIR}" ] && { echo "- SECO firmware already downloaded"; return 0; }

	(
		cd "${BASEDIR}" || { echo "download_firmware_seco: BASEDIR not found"; exit 1; }
		if [ ! -f "${IMX_SECO}.bin" ]; then
			if ! wget "${IMX_SECO_URL}"; then
				echo "- Unable to download SECO firmware from ${IMX_SECO_URL}"
				exit 1
			fi
		fi
		sh "${IMX_SECO}.bin" --auto-accept --force
	)
}

copy_artifacts_mkimage_folder()
{
	( cd "${MKIMAGE_DIR}" && git clean -ffdx )
	echo "- Copy U-Boot binaries from ${UBOOT_DIR}"
	if [ ! -d "${UBOOT_DIR}" ]; then
		echo "- Missing u-boot directory: ${UBOOT_DIR}"
		exit 1
	elif [ -f "${UBOOT_DIR}/u-boot.bin" ] && [ -f "${UBOOT_DIR}/spl/u-boot-spl.bin" ]; then
		cp --remove-destination "${UBOOT_DIR}"/u-boot.bin "${MKIMAGE_DIR}"/"${SOC}"
		cp --remove-destination "${UBOOT_DIR}"/spl/u-boot-spl.bin "${MKIMAGE_DIR}"/"${SOC}"
	else
		echo "- Missing u-boot artifacts"
		exit 1
	fi

	# Digi SC firmware
	cp --remove-destination "${DIGI_SC_FW_DIR}"/mx8x-ccimx8x-scfw-tcm.bin "${MKIMAGE_DIR}"/"${SOC}"/scfw_tcm.bin

	# AHAB container
	cp --remove-destination "${IMX_SECO_DIR}"/firmware/seco/mx8qxb0-ahab-container.img "${MKIMAGE_DIR}"/"${SOC}"
	cp --remove-destination "${IMX_SECO_DIR}"/firmware/seco/mx8qxc0-ahab-container.img "${MKIMAGE_DIR}"/"${SOC}"

	# ATF
	cp --remove-destination "${ATF_DIR}"/build/"${ATF_PLAT}"/release/bl31.bin "${MKIMAGE_DIR}"/"${SOC}"
}

build_imxboot()
{
	cd "${MKIMAGE_DIR}" || exit 1
	mkdir -p "${OUTPUT_PATH}"

	(
		for rev in B0 C0; do
			echo "- Build imx-boot binary for: ${SOC} (${rev})"
			${MAKE} SOC="${SOC}" REV="${rev}" clean
			${MAKE} SOC="${SOC}" REV="${rev}" flash_spl
			cp --remove-destination "${SOC}"/flash.bin "${OUTPUT_PATH}"/imx-boot-ccimx8x-sbc-pro-${rev}.bin
			cp --remove-destination "${SOC}"/mkimage-flash_spl.log "${OUTPUT_PATH}"/mkimage-ccimx8x-sbc-pro-${rev}-flash_spl.log
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
	mkimage/0001-iMX8QX-soc.mak-capture-commands-output-into-a-log-fi.patch \
"

ATF_REPO="https://github.com/nxp-imx/imx-atf.git"
ATF_BRANCH="lf_v2.8"
# Tag: lf-6.1.55-2.2.0
ATF_REV="08e9d4eef2262c0dd072b4325e8919e06d349e02"
ATF_DIR="${BASEDIR}/imx-atf"

DIGI_SC_FW="digi-sc-firmware-1.15.0.1"
DIGI_SC_FW_DIR="${BASEDIR}/${DIGI_SC_FW}"
DIGI_SC_FW_URL="https://ftp1.digi.com/support/digiembeddedyocto/source/${DIGI_SC_FW}.tar.gz"

IMX_SECO="imx-seco-5.9.2"
IMX_SECO_DIR="${BASEDIR}/${IMX_SECO}"
IMX_SECO_URL="https://www.nxp.com/lgfiles/NMG/MAD/YOCTO/${IMX_SECO}.bin"

SOC="iMX8QX"
ATF_PLAT="imx8qx"

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
download_digi_sc_firmware
download_firmware_seco
clone_atf_repo
patch_atf_repo
build_atf
clone_mkimage_repo
patch_mkimage_repo
copy_artifacts_mkimage_folder
build_imxboot
