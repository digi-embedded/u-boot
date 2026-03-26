// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2026, Digi International, Inc.
 */

#ifndef __SMARCID_CCIMX95_H_
#define __SMARCID_CCIMX95_H_

/*
 * SMARC HWID is stored in 2 Fuse Words, being:
 *
 *              COM_DEVICE_ID_CFG3[31:0] (Bank 40 Word 5)
 *
 *   | 31..24 |   23   |  22  | 21  | 20  | 19  |  18  |  17  | 16  |  15..13  |  12..8  |7..4|  3..0 |
 *   +--------+--------+------+-----+-----+-----+------+------+-----+----------+---------+----+-------+
 *   |   --   | EEPROM | TEMP | RTC | TPM | HUB | ETH2 | ETH1 | MCA | Location | Variant | HV | MAGIC |
 *   +--------+--------+------+-----+-----+-----+------+------+-----+----------+---------+----+-------+
 *
 *              COM_DEVICE_ID_CFG4[31:0] (Bank 40 Word 6)
 *
 *   |  31..26  |  25..20  |            19..0           |
 *   +----------+----------+----------------------------+
 *   |   Year   |   Week   |        Serial number       |
 *   +----------+----------+----------------------------+
 */
struct __packed digi_smarcid {
	/* Word 0 */
	u32	magic:4;	/* SMARC MAGIC field (0xD) */
	u32	hv:4;		/* hardware version */
	u32	variant:5;	/* module variant */
	u32	location:3;	/* location */
	u32	mca:1;		/* has MCA */
	u32	eth1:1;		/* has ETH1 */
	u32	eth2:1;		/* has ETH2 */
	u32	hub:1;		/* has USB Hub */
	u32	tpm:1;		/* has TPM module */
	u32	rtc:1;		/* has RTC module */
	u32	temp:1;		/* has Temp sensor */
	u32	eeprom:1;	/* has EEPROM chip */
	u32	spare:8;	/* spare */
	/* Word 1 */
	u32	sn:20;		/* serial number */
	u32	week:6;		/* manufacturing week */
	u32	year:6;		/* manufacturing year */
}__aligned(4);

#define CONFIG_SMARCID_STRINGS_HELP		"<XXXXXXXX> <YYYYYYYY>"
#define CONFIG_SMARCID_MANUF_STRINGS_HELP	"<LYYWWFFXXXXXX> <VVH> <MEEUTRSA>"

#define SMARC_MAGIC	0xD

#endif /* __SMARCID_CCIMX95_H_ */
