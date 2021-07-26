#ifndef CCIMX8MM_DVK_ANDROID_H
#define CCIMX8MM_DVK_ANDROID_H

#define CONFIG_ANDROID_AB_SUPPORT
#ifdef CONFIG_ANDROID_AB_SUPPORT
#define CONFIG_SYSTEM_RAMDISK_SUPPORT
#endif

/* Enable mcu firmware flash */
#ifdef CONFIG_FLASH_MCUFIRMWARE_SUPPORT
#define ANDROID_MCU_FRIMWARE_DEV_TYPE DEV_MMC
#define ANDROID_MCU_FIRMWARE_START 0x500000
#define ANDROID_MCU_FIRMWARE_SIZE  0x40000
#define ANDROID_MCU_FIRMWARE_HEADER_STACK 0x20020000
#endif

/* Fastboot BCB support uses 'do_raw_read' */
#define CONFIG_CMD_READ

/* Empty bootcmd to boot Android automatically */
#undef CONFIG_BOOTCOMMAND

/*
 * Do not use encrypted lock
 *
 * This setting controls whether fastboot lock status is encrypted or not
 * using the CAAM. At the moment using the CAAM for such task is failing
 * (in CAAM-based 'decrypt_lock_store' function).
 */
#define NON_SECURE_FASTBOOT

#endif /* CCIMX8MM_DVK_ANDROID_H */
