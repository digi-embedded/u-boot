#ifndef CCIMX8X_SBC_PRO_ANDROID_H
#define CCIMX8X_SBC_PRO_ANDROID_H

#define CONFIG_ANDROID_AB_SUPPORT
#ifdef CONFIG_ANDROID_AB_SUPPORT
#define CONFIG_SYSTEM_RAMDISK_SUPPORT
#endif

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

#endif /* CCIMX8X_SBC_PRO_ANDROID_H */
