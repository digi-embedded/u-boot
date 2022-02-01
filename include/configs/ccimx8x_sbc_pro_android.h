#ifndef CCIMX8X_SBC_PRO_ANDROID_H
#define CCIMX8X_SBC_PRO_ANDROID_H

#define CONFIG_USB_GADGET_VBUS_DRAW	2

/* Disable some configs not needed for Android */
#undef CONFIG_BOOTCOMMAND
#undef CONFIG_FSL_CAAM_KB

#ifdef CONFIG_IMX_TRUSTY_OS
#define AVB_RPMB
#define NS_ARCH_ARM64 1
#define KEYSLOT_HWPARTITION_ID   2
#define KEYSLOT_BLKS             0x3FFF

#ifdef CONFIG_SPL_BUILD
#undef CONFIG_BLK
#define CONFIG_FSL_CAAM_KB
#endif
#endif

#endif /* CCIMX8X_SBC_PRO_ANDROID_H */
