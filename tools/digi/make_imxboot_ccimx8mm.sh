#!/bin/sh

set -e

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
		${MAKE} CROSS_COMPILE=${CROSS_COMPILE} LD=${CROSS_COMPILE}ld.bfd CC=${CROSS_COMPILE}gcc PLAT="${ATF_PLAT}" realclean
		${MAKE} CROSS_COMPILE=${CROSS_COMPILE} LD=${CROSS_COMPILE}ld.bfd CC=${CROSS_COMPILE}gcc PLAT="${ATF_PLAT}" bl31
		${MAKE} CROSS_COMPILE=${CROSS_COMPILE} LD=${CROSS_COMPILE}ld.bfd CC=${CROSS_COMPILE}gcc PLAT="${ATF_PLAT}" BUILD_BASE=build-optee realclean
		${MAKE} CROSS_COMPILE=${CROSS_COMPILE} LD=${CROSS_COMPILE}ld.bfd CC=${CROSS_COMPILE}gcc PLAT="${ATF_PLAT}" BUILD_BASE=build-optee SPD=opteed bl31
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
		${MAKE} PLATFORM=imx-ccimx8mmdvk \
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

copy_artifacts_mkimage_folder()
{
	( cd "${MKIMAGE_DIR}" && git clean -ffdx )
	echo "- Copy U-Boot binaries from ${UBOOT_DIR}"
	if [ ! -d "${UBOOT_DIR}" ]; then
		echo "- Missing u-boot directory: ${UBOOT_DIR}"
		exit 1
	elif [ -f "${UBOOT_DIR}/u-boot-nodtb.bin" ] && [ -f "${UBOOT_DIR}/spl/u-boot-spl.bin" ] \
		&& [ -f "${UBOOT_DIR}/arch/arm/dts/ccimx8mm-dvk.dtb" ] && [ -f "${UBOOT_DIR}/tools/mkimage" ]; then
		cp --remove-destination "${UBOOT_DIR}"/u-boot-nodtb.bin "${MKIMAGE_SOC_DIR}"
		cp --remove-destination "${UBOOT_DIR}"/spl/u-boot-spl.bin "${MKIMAGE_SOC_DIR}"
		cp --remove-destination "${UBOOT_DIR}"/arch/arm/dts/ccimx8mm-dvk.dtb "${MKIMAGE_SOC_DIR}"
		cp --remove-destination "${UBOOT_DIR}"/tools/mkimage "${MKIMAGE_SOC_DIR}"/mkimage_uboot
	else
		echo "- Missing u-boot artifacts"
		exit 1
	fi

	# LPDDR4 training files
	(
		cd "${FIRMWARE_IMX_DIR}"/firmware/ddr/synopsys || exit 1
		cp --remove-destination lpddr4_pmu_train_1d_imem.bin lpddr4_pmu_train_1d_dmem.bin \
			lpddr4_pmu_train_2d_imem.bin lpddr4_pmu_train_2d_dmem.bin "${MKIMAGE_SOC_DIR}"
	)

	# ATF
	cp --remove-destination "${ATF_DIR}"/build/"${ATF_PLAT}"/release/bl31.bin "${MKIMAGE_SOC_DIR}"/bl31-imx8mm.bin
	cp --remove-destination "${ATF_DIR}"/build-optee/"${ATF_PLAT}"/release/bl31.bin "${MKIMAGE_SOC_DIR}"/bl31-imx8mm.bin-optee

	# OPTEE binary
	cp --remove-destination "${OPTEE_DIR}"/build/core/tee-raw.bin "${MKIMAGE_SOC_DIR}"

	# Create dummy DEK blob to support building an encrypted imx-boot
	dd if=/dev/zero of="${MKIMAGE_SOC_DIR}"/dek_blob_fit_dummy.bin bs=96 count=1 oflag=sync
}

build_imxboot()
{
	cd "${MKIMAGE_DIR}" || exit 1
	mkdir -p "${OUTPUT_PATH}"

	(
		echo "- Build imx-boot (NO-OPTEE) binary for: ${SOC}"
		${MAKE} SOC="${SOC}" clean
		[ -f "${MKIMAGE_SOC_DIR}"/bl31-imx8mm.bin ] && ln -sf bl31-imx8mm.bin "${MKIMAGE_SOC_DIR}"/bl31.bin
		rm -f "${MKIMAGE_SOC_DIR}"/tee.bin
		${MAKE} SOC="${SOC}" dtbs=ccimx8mm-dvk.dtb flash_evk
		${MAKE} SOC="${SOC}" dtbs=ccimx8mm-dvk.dtb print_fit_hab
		cp --remove-destination "${MKIMAGE_SOC_DIR}"/flash.bin "${OUTPUT_PATH}"/imx-boot-ccimx8mm_dvk-nooptee.bin
		cp --remove-destination "${MKIMAGE_SOC_DIR}"/mkimage-flash_evk.log "${OUTPUT_PATH}"/mkimage-ccimx8mm_dvk-nooptee-flash_evk.log
		cp --remove-destination "${MKIMAGE_SOC_DIR}"/mkimage-print_fit_hab.log "${OUTPUT_PATH}"/mkimage-ccimx8mm_dvk-nooptee-print_fit_hab.log

		echo "- Build imx-boot (OPTEE) binary for: ${SOC}"
		${MAKE} SOC="${SOC}" clean
		[ -f "${MKIMAGE_SOC_DIR}"/bl31-imx8mm.bin-optee ] && ln -sf bl31-imx8mm.bin-optee "${MKIMAGE_SOC_DIR}"/bl31.bin
		[ -f "${MKIMAGE_SOC_DIR}"/tee-raw.bin ] && ln -sf tee-raw.bin "${MKIMAGE_SOC_DIR}"/tee.bin
		${MAKE} SOC="${SOC}" dtbs=ccimx8mm-dvk.dtb flash_evk
		${MAKE} SOC="${SOC}" dtbs=ccimx8mm-dvk.dtb print_fit_hab
		cp --remove-destination "${MKIMAGE_SOC_DIR}"/flash.bin "${OUTPUT_PATH}"/imx-boot-ccimx8mm_dvk.bin
		cp --remove-destination "${MKIMAGE_SOC_DIR}"/mkimage-flash_evk.log "${OUTPUT_PATH}"/mkimage-ccimx8mm_dvk-flash_evk.log
		cp --remove-destination "${MKIMAGE_SOC_DIR}"/mkimage-print_fit_hab.log "${OUTPUT_PATH}"/mkimage-ccimx8mm_dvk-print_fit_hab.log
	)
}

sign_imxboot()
{
	[ -z "${CONFIG_SIGN_KEYS_PATH}" ] && return

	# Signing environment
	TF_SIGN_BASE_ENV="CONFIG_SIGN_KEYS_PATH=${CONFIG_SIGN_KEYS_PATH}"
	[ -n "${CONFIG_KEY_INDEX}" ] && TF_SIGN_BASE_ENV="${TF_SIGN_BASE_ENV} CONFIG_KEY_INDEX=${CONFIG_KEY_INDEX}"
	[ -n "${CONFIG_UNLOCK_SRK_REVOKE}" ] && TF_SIGN_BASE_ENV="${TF_SIGN_BASE_ENV} CONFIG_UNLOCK_SRK_REVOKE=${CONFIG_UNLOCK_SRK_REVOKE}"

	# Encryption environment
	TF_ENC_ENV="CONFIG_DEK_PATH=${CONFIG_SIGN_KEYS_PATH}/dek.bin ENABLE_ENCRYPTION=y"

	(
		cd "${OUTPUT_PATH}" || exit 1

		echo "- Sign and encrypt imx-boot (NO-OPTEE) binary for: ${SOC}"
		TF_SIGN_ENV="${TF_SIGN_BASE_ENV} CONFIG_MKIMAGE_LOG_PATH=mkimage-ccimx8mm_dvk-nooptee-flash_evk.log CONFIG_FIT_HAB_LOG_PATH=mkimage-ccimx8mm_dvk-nooptee-print_fit_hab.log"
		env ${TF_SIGN_ENV} "${SIGN_SCRIPT}" imx-boot-ccimx8mm_dvk-nooptee.bin imx-boot-signed-ccimx8mm_dvk-nooptee.bin
		env ${TF_SIGN_ENV} ${TF_ENC_ENV} "${SIGN_SCRIPT}" imx-boot-ccimx8mm_dvk-nooptee.bin imx-boot-encrypted-ccimx8mm_dvk-nooptee.bin

		echo "- Sign and encrypt imx-boot (OPTEE) binary for: ${SOC}"
		TF_SIGN_ENV="${TF_SIGN_BASE_ENV} CONFIG_MKIMAGE_LOG_PATH=mkimage-ccimx8mm_dvk-flash_evk.log CONFIG_FIT_HAB_LOG_PATH=mkimage-ccimx8mm_dvk-print_fit_hab.log"
		env ${TF_SIGN_ENV} "${SIGN_SCRIPT}" imx-boot-ccimx8mm_dvk.bin imx-boot-signed-ccimx8mm_dvk.bin
		env ${TF_SIGN_ENV} ${TF_ENC_ENV} "${SIGN_SCRIPT}" imx-boot-ccimx8mm_dvk.bin imx-boot-encrypted-ccimx8mm_dvk.bin
	)
}

##### Main
BASEDIR="$(cd "$(dirname "$0")" && pwd)"

MKIMAGE_REPO="https://github.com/nxp-imx/imx-mkimage.git"
MKIMAGE_BRANCH="lf-6.6.52_2.2.1"
# Tag: lf-6.6.52-2.2.1
MKIMAGE_REV="81fca6434be0610f3f9216a762aadc4dc3e8d8db"
MKIMAGE_DIR="${BASEDIR}/imx-mkimage"
MKIMAGE_SOC_DIR="${MKIMAGE_DIR}/iMX8M"
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

FIRMWARE_IMX="firmware-imx-8.26.1-410be01"
FIRMWARE_IMX_DIR="${BASEDIR}/${FIRMWARE_IMX}"
FIRMWARE_IMX_URL="https://www.nxp.com/lgfiles/NMG/MAD/YOCTO/${FIRMWARE_IMX}.bin"

SOC="iMX8MM"
ATF_PLAT="imx8mm"

OUTPUT_PATH="${BASEDIR}/output"
UBOOT_DIR="${UBOOT_DIR:-$(realpath "${BASEDIR}"/../..)}"
SIGN_SCRIPT="${UBOOT_DIR}/scripts/sign_spl_fit.sh"

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
sign_imxboot
