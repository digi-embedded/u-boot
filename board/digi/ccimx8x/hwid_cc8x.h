/*
 * Copyright 2014-2019 Digi International Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef __HWID_CC8X_H_
#define __HWID_CC8X_H_

/*
 * On the CC8X the HWID is stored in 4 consecutive Fuse Words, being:
 *
 *              MAC1[31:0] (Bank 0 Word 708)
 *
 *   | 31..30 | 29..24 | 23..20 |         19..0         |
 *   +--------+--------+--------+-----------------------+
 *   |   WID  |  Year  | Month  |     Serial number     |
 *   +--------+--------+--------+-----------------------+
 *
 *              MAC1[47:32] (Bank 0 Word 709)
 *
 *   |     31..16      | 15  | 14..11 |  10..6  | 5..3 | 2..0 |
 *   +-----------------+-----+--------+---------+------+------+
 *   |     RESERVED    | MCA |   RAM  | Variant |  HV  | Cert |
 *   +-----------------+-----+--------+---------+------+------+
 *
 *              MAC2[31:0] (Bank 0 Word 710)
 *
 *   | 31..28 |  27..24  |             23..0            |
 *   +--------+----------+------------------------------+
 *   |  GenID | MAC pool |            MAC base          |
 *   +--------+----------+------------------------------+
 *
 *              MAC2[47:32] (Bank 0 Word 711)
 *
 *   |     31..16      | 15..9 |  8..3  |    2   | 1  |   0   |
 *   +-----------------+----------------+--------+----+-------+
 *   |     RESERVED    |   --  |  Week  | Crypto | BT | Wi-Fi |
 *   +-----------------+----------------+--------+----+-------+
 *
 */
struct __packed digi_hwid {
	u32	sn:20;		/* serial number */
	u32	month:4;	/* manufacturing month */
	u32	year:6;		/* manufacturing year */
	u32	wid:2;		/* wireless ID */
	u32	cert:3;		/* type of wifi certification */
	u32	hv:3;		/* hardware version */
	u32	variant:5;	/* module variant */
	u32	ram:4;		/* RAM */
	u32	mca:1;		/* has MCA */
	u32	reserved:16;	/* reserved by NXP */
	u32	mac_base:24;	/* MAC base address */
	u32	mac_pool:4;	/* MAC address pool */
	u32	genid:4;	/* generator id */
	u32	wifi:1;		/* has Wi-Fi */
	u32	bt:1;		/* has Bluetooth */
	u32	crypto:1;	/* has crypto-authentication */
	u32	week:6;		/* manufacturing week */
	u32	spare:7;	/* spare */
	u32	reserved2:16;	/* reserved by NXP */
}__aligned(4);

enum imx8_cpu {
	IMX8_NONE = 0,	/* Reserved */
	IMX8QXP,	/* QuadXPlus */
	IMX8DX,		/* DualX */
};

struct ccimx8_variant {
	enum imx8_cpu		cpu;
	const phys_size_t	sdram;
	u8			capabilities;
	const char		*id_string;
};

/* Capabilities */
#define	CCIMX8_HAS_WIRELESS	(1 << 0)
#define	CCIMX8_HAS_BLUETOOTH	(1 << 1)

#define CONFIG_HWID_STRINGS_HELP	"<WWWW> <XXXXXXXX> <YYYY> <ZZZZZZZZ>"
#define CONFIG_MANUF_STRINGS_HELP	"<YYWWGGXXXXXX> <PPAAAAAA> <VVHC> <K> <RMWBC>"
#define DIGICMD_HWID_SUPPORTED_OPTIONS_HELP \
	     "read - sense HWID from fuses\n" \
	"hwid read_manuf - sense HWID from fuses and print manufacturing ID\n" \
	"hwid sense - sense HWID from fuses\n" \
	"hwid sense_manuf - sense HWID from fuses and print manufacturing ID\n" \
	"hwid prog [-y] " CONFIG_HWID_STRINGS_HELP " - program HWID (PERMANENT)\n" \
	"hwid prog_manuf [-y] " CONFIG_MANUF_STRINGS_HELP " - program HWID with manufacturing ID (PERMANENT)\n"

#endif	/* __HWID_CC8X_H_ */
