LOCAL_PATH := $(call my-dir)

# trustfence-sign-uboot.sh script
# ============================================================
$(HOST_OUT_EXECUTABLES)/trustfence-sign-uboot.sh: | \
	$(HOST_OUT_EXECUTABLES)/csf_templates/encrypt_uboot \
	$(HOST_OUT_EXECUTABLES)/csf_templates/sign_uboot

include $(CLEAR_VARS)
LOCAL_PREBUILT_EXECUTABLES := trustfence-sign-uboot:sign.sh
include $(BUILD_HOST_PREBUILT)

# CSF u-boot sign and encrypt templates
# ============================================================
include $(CLEAR_VARS)
LOCAL_PREBUILT_EXECUTABLES :=  \
	csf_templates/encrypt_uboot:csf_templates/encrypt_uboot \
	csf_templates/sign_uboot:csf_templates/sign_uboot
include $(BUILD_HOST_PREBUILT)
