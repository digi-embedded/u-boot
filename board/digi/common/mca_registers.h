/*
 *  Copyright 2016-2018 Digi International Inc
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#ifndef MCA_REGISTERS_H_
#define MCA_REGISTERS_H_


/* EP0: Control and status */
#define MCA_DEVICE_ID			0x0001
#define MCA_HW_VER			0x0002
#define MCA_FW_VER_L			0x0003
#define MCA_FW_VER_H			0x0004
#define MCA_UID_0			0x0005
#define MCA_UID_1			0x0006
#define MCA_UID_2			0x0007
#define MCA_UID_3			0x0008
#define MCA_UID_4			0x0009
#define MCA_UID_5			0x000A
#define MCA_UID_6			0x000B
#define MCA_UID_7			0x000C
#define MCA_UID_8			0x000D
#define MCA_UID_9			0x000E
#define MCA_HWVER_SOM			0x000F

#define MCA_IRQ_STATUS_0		0x0020
#define MCA_IRQ_STATUS_1		0x0021
#define MCA_IRQ_STATUS_2		0x0022
#define MCA_IRQ_STATUS_3		0x0023
#define MCA_IRQ_MASK_0			0x0024
#define MCA_IRQ_MASK_1			0x0025
#define MCA_IRQ_MASK_2			0x0026
#define MCA_IRQ_MASK_3			0x0027

#define MCA_PWR_CTRL_0			0x0028
#define MCA_PWR_STATUS_0		0x0029
#define MCA_PWR_BUT_DEBOUNCE		0x002a
#define MCA_PWR_BUT_DELAY		0x002b
#define MCA_PWR_BUT_GUARD		0x002c

#define MCA_CTRL_UNLOCK_0		0x002d
#define MCA_CTRL_UNLOCK_1		0x002e
#define MCA_CTRL_UNLOCK_2		0x002f
#define MCA_CTRL_UNLOCK_3		0x0030
#define MCA_CTRL_0			0x0031

#define MCA_TAMPER0_CFG0		0x0037
#define MCA_TAMPER0_CFG1		0x0038
#define MCA_TAMPER0_IO_IN		0x0039
#define MCA_TAMPER0_IO_OUT		0x003a
#define MCA_TAMPER0_DELAY_PWROFF	0x003b
#define MCA_TAMPER0_DATE_START		0x003c
#define MCA_TAMPER0_EVENT		0x0045
#define MCA_TAMPER1_CFG0		0x0046
#define MCA_TAMPER1_CFG1		0x0047
#define MCA_TAMPER1_IO_IN		0x0048
#define MCA_TAMPER1_IO_OUT		0x0049
#define MCA_TAMPER1_DELAY_PWROFF	0x004a
#define MCA_TAMPER1_DATE_START		0x004b
#define MCA_TAMPER1_EVENT		0x0054

#define MCA_TAMPER2_CFG0		0x0060
#define MCA_TAMPER2_CFG1		0x0061
#define MCA_TAMPER2_IO_IN		0x0062
#define MCA_TAMPER2_IO_OUT		0x0063
#define MCA_TAMPER2_DELAY_PWROFF	0x0064
/* 1 byte for padding */
/* Tamper event time-stamp follows the same format used by the RTC registers*/
#define MCA_TAMPER2_DATE_START		0x0066
/* ... */
#define MCA_TAMPER2_DATE_END		0x006C
/* 1 byte for padding */
#define MCA_TAMPER2_EVENT		0x006E
/* 1 byte for padding */
#define MCA_TAMPER2_TICKS_L		0x0070
#define MCA_TAMPER2_TICKS_H		0x0071
#define MCA_TAMPER2_THRESH_LO_L		0x0072
#define MCA_TAMPER2_THRESH_LO_H		0x0073
#define MCA_TAMPER2_THRESH_HI_L		0x0074
#define MCA_TAMPER2_THRESH_HI_H		0x0075

#define MCA_TAMPER3_CFG0		0x0080
#define MCA_TAMPER3_CFG1		0x0081
#define MCA_TAMPER3_IO_IN		0x0082
#define MCA_TAMPER3_IO_OUT		0x0083
#define MCA_TAMPER3_DELAY_PWROFF	0x0084
/* 1 byte for padding */
/* Tamper event time-stamp follows the same format used by the RTC registers*/
#define MCA_TAMPER3_DATE_START		0x0086
/* ... */
#define MCA_TAMPER3_DATE_END		0x008C
/* 1 byte for padding */
#define MCA_TAMPER3_EVENT		0x008E
/* 1 byte for padding */
#define MCA_TAMPER3_TICKS_L		0x0090
#define MCA_TAMPER3_TICKS_H		0x0091
#define MCA_TAMPER3_THRESH_LO_L		0x0092
#define MCA_TAMPER3_THRESH_LO_H		0x0093
#define MCA_TAMPER3_THRESH_HI_L		0x0094
#define MCA_TAMPER3_THRESH_HI_H		0x0095

#define MCA_TAMPER_REG_LEN		(MCA_TAMPER1_CFG0 - \
					 MCA_TAMPER0_CFG0)

#define MCA_TAMPER_DATE_LEN		(MCA_TAMPER0_EVENT - \
					 MCA_TAMPER0_DATE_START)

#define MCA_TAMPER_DATE_OFF		(MCA_TAMPER0_DATE - \
					 MCA_TAMPER0_CFG0)

#define MCA_TAMPER_CFG0_OFFSET		(MCA_TAMPER0_CFG0 - \
					 MCA_TAMPER0_CFG0)

#define MCA_TAMPER_EV_OFFSET		(MCA_TAMPER0_EVENT - \
					 MCA_TAMPER0_CFG0)


/* EP1: RTC */
#define MCA_RTC_CONTROL			0x0101

#define MCA_RTC_COUNT_YEAR_L		0x0103
#define MCA_RTC_COUNT_YEAR_H		0x0104
#define MCA_RTC_COUNT_MONTH		0x0105
#define MCA_RTC_COUNT_DAY		0x0106
#define MCA_RTC_COUNT_HOUR		0x0107
#define MCA_RTC_COUNT_MIN		0x0108
#define MCA_RTC_COUNT_SEC		0x0109
#define MCA_RTC_ALARM_YEAR_L		0x010A
#define MCA_RTC_ALARM_YEAR_H		0x010B
#define MCA_RTC_ALARM_MONTH		0x010C
#define MCA_RTC_ALARM_DAY		0x010D
#define MCA_RTC_ALARM_HOUR		0x010E
#define MCA_RTC_ALARM_MIN		0x010F
#define MCA_RTC_ALARM_SEC		0x0110

/* EP2: Watchdog */
#define MCA_WDT_CONTROL			0x0201
#define MCA_WDT_TIMEOUT			0x0202
#define MCA_WDT_REFRESH_0		0x0203
#define MCA_WDT_REFRESH_1		0x0204
#define MCA_WDT_REFRESH_2		0x0205
#define MCA_WDT_REFRESH_3		0x0206

#define MCA_GPIO_WDT0_CONTROL		0x0210
#define MCA_GPIO_WDT0_TIMEOUT		0x0211
#define MCA_GPIO_WDT0_IO		0x0212

#define MCA_GPIO_WDT1_CONTROL		0x0220
#define MCA_GPIO_WDT1_TIMEOUT		0x0221
#define MCA_GPIO_WDT1_IO		0x0222

#define MCA_GPIO_WDT2_CONTROL		0x0230
#define MCA_GPIO_WDT2_TIMEOUT		0x0231
#define MCA_GPIO_WDT2_IO		0x0232

#define MCA_GPIO_WDT3_CONTROL		0x0240
#define MCA_GPIO_WDT3_TIMEOUT		0x0241
#define MCA_GPIO_WDT3_IO		0x0242


/* EP3: GPIO */
#define MCA_GPIO_NUM			0x0301
#define MCA_GPIO_DIR_0			0x0302
#define MCA_GPIO_DIR_1			0x0303
#define MCA_GPIO_DIR_2			0x0304
#define MCA_GPIO_DIR_3			0x0305
#define MCA_GPIO_DIR_4			0x0306
#define MCA_GPIO_DIR_5			0x0307
#define MCA_GPIO_DIR_6			0x0308
#define MCA_GPIO_DIR_7			0x0309
#define MCA_GPIO_DATA_0			0x030A
#define MCA_GPIO_DATA_1			0x030B
#define MCA_GPIO_DATA_2			0x030C
#define MCA_GPIO_DATA_3			0x030D
#define MCA_GPIO_DATA_4			0x030E
#define MCA_GPIO_DATA_5			0x030F
#define MCA_GPIO_DATA_6			0x0310
#define MCA_GPIO_DATA_7			0x0311
#define MCA_GPIO_SET_0			0x0312
#define MCA_GPIO_SET_1			0x0313
#define MCA_GPIO_SET_2			0x0314
#define MCA_GPIO_SET_3			0x0315
#define MCA_GPIO_SET_4			0x0316
#define MCA_GPIO_SET_5			0x0317
#define MCA_GPIO_SET_6			0x0318
#define MCA_GPIO_SET_7			0x0319
#define MCA_GPIO_CLEAR_0		0x031A
#define MCA_GPIO_CLEAR_1		0x031B
#define MCA_GPIO_CLEAR_2		0x031C
#define MCA_GPIO_CLEAR_3		0x031D
#define MCA_GPIO_CLEAR_4		0x031E
#define MCA_GPIO_CLEAR_5		0x031F
#define MCA_GPIO_CLEAR_6		0x0320
#define MCA_GPIO_CLEAR_7		0x0321
#define MCA_GPIO_TOGGLE_0		0x0322
#define MCA_GPIO_TOGGLE_1		0x0323
#define MCA_GPIO_TOGGLE_2		0x0324
#define MCA_GPIO_TOGGLE_3		0x0325
#define MCA_GPIO_TOGGLE_4		0x0326
#define MCA_GPIO_TOGGLE_5		0x0327
#define MCA_GPIO_TOGGLE_6		0x0328
#define MCA_GPIO_TOGGLE_7		0x0329

/*
 * MCA registers bitfields
 */

/* MCA_IRQ_STATUS_0 (addr=0x0020) */
#define MCA_RTC_ALARM			BIT(0)
#define MCA_RTC_1HZ			BIT(1)
#define MCA_WATCHDOG			BIT(2)

/* MCA_IRQ_MASK_0 (addr=0x0024) */
#define MCA_M_RTC_ALARM			MCA_RTC_ALARM
#define MCA_M_RTC_1HZ			MCA_RTC_1HZ
#define MCA_M_WATCHDOG			MCA_WATCHDOG

#define MCA_CTRL_0_RESET		BIT(0)
#define MCA_CTRL_0_SAVE_CFG		BIT(3)

/* MCA_TAMPER CFGn (addr=0x0037 && addr 0x0046) */
#define MCA_TAMPER_DET_EN		BIT(0)

/* MCA_TAMPER EVENT (addr=0x0045) */
#define MCA_TAMPER_SIGNALED		BIT(0)
#define MCA_TAMPER_ACKED		BIT(1)

/* MCA_RTC_CONTROL (addr=0x0101) */
#define MCA_RTC_EN			BIT(0)
#define MCA_RTC_ALARM_EN		BIT(1)
#define MCA_RTC_1HZ_EN			BIT(2)
#define MCA_RTC_32KHZ_OUT_EN		BIT(3)

/* MCA_RTC_COUNT_YEAR_L (addr=0x0103) */
/* MCA_RTC_ALARM_YEAR_L (addr=0x010A) */
#define MCA_RTC_YEAR_L_MASK		0xFF

/* MCA_RTC_COUNT_YEAR_H (addr=0x0104) */
/* MCA_RTC_ALARM_YEAR_H (addr=0x010B) */
#define MCA_RTC_YEAR_H_MASK		0xFF

/* MCA_RTC_COUNT_MONTH (addr=0x0105) */
/* MCA_RTC_ALARM_MONTH (addr=0x010C) */
#define MCA_RTC_MONTH_MASK		0x0F

/* MCA_RTC_COUNT_DAY (addr=0x0106) */
/* MCA_RTC_ALARM_DAY (addr=0x010D) */
#define MCA_RTC_DAY_MASK		0x1F

/* MCA_RTC_COUNT_HOUR (addr=0x0107) */
/* MCA_RTC_ALARM_HOUR (addr=0x010E) */
#define MCA_RTC_HOUR_MASK		0x1F

/* MCA_RTC_COUNT_MIN (addr=0x0108) */
/* MCA_RTC_ALARM_MIN (addr=0x010F) */
#define MCA_RTC_MIN_MASK		0x3F

/* MCA_RTC_COUNT_SEC (addr=0x0109) */
/* MCA_RTC_ALARM_SEC (addr=0x0110) */
#define MCA_RTC_SEC_MASK		0x3F

/* MCA_WDT_CONTROL (addr=0x0201) */
#define MCA_WDT_ENABLE			BIT(0)
#define MCA_WDT_NOWAYOUT		BIT(1)
#define MCA_WDT_IRQNORESET		BIT(2)
#define MCA_WDT_PRETIMEOUT		BIT(3)
#define MCA_WDT_FULLRESET		BIT(4)

/* MCA_GPIO_WDT_CONTROL */
#define MCA_GPIO_WDT_MODE_SHIFT		0x5
#define MCA_GPIO_WDT_MODE_MASK		(0x3 << MCA_GPIO_WDT_MODE_SHIFT)

/* MCA_WDT_TIMEOUT (addr=0x0202) */
#define MCA_WDT_TIMEOUT_MASK		0xFF

/* MCA_WDT_REFRESH_X (addr=0x0203..0x0206) */
#define MCA_WDT_REFRESH_X_MASK		0xFF

/* MCA_GPIO_NUM (addr=0x0302) */
#define MCA_GPIO_NUM_MASK		0x7F

#endif /* MCA_REGISTERS_H_ */
