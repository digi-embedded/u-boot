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

#ifndef __HWID_CC6_H_
#define __HWID_CC6_H_

/*
 * HWID is stored in 2 consecutive Fuse Words, being:
 *
 *                MAC 0 (Bank 4 Word 2)
 *
 *   |    31..27   | 26..20  |          19..0           |
 *   +-------------+---------+--------------------------+
 *   |   Location  |  GenID  |      Serial number       |
 *   +-------------+---------+--------------------------+
 *
 *                MAC 1 (Bank 4 Word 3)
 *
 *   | 31..26 | 25..20 | 19..16 |  15..8  | 7..4 | 3..0 |
 *   +--------+--------+--------+---------+------+------+
 *   |  Year  |  Week  |  WID   | Variant |  HV  | Cert |
 *   +--------+--------+--------+---------+------+------+
 */
struct __packed digi_hwid {
	u32	sn:20;		/* serial number */
	u32	genid:7;	/* generator id */
	u32	location:5;	/* location */
	u32	cert:4;		/* type of wifi certification */
	u32	hv:4;		/* hardware version */
	u32	variant:8;	/* module variant */
	u32	wid:4;		/* wireless ID */
	u32	week:6;		/* manufacturing week */
	u32	year:6;		/* manufacturing year */
}__aligned(4);

enum imx6_cpu {
	IMX6_NONE = 0,	/* Reserved */
	IMX6Q,		/* Quad */
	IMX6D,		/* Dual */
	IMX6DL,		/* Dual-Lite */
	IMX6S,		/* Solo */
	IMX6UL,		/* UltraLite */
	IMX6QP,		/* QuadPlus */
	IMX6DP,		/* DualPlus */
};

struct ccimx6_variant {
	enum imx6_cpu	cpu;
	const int	sdram;
	u8		capabilities;
	const char	*id_string;
};

/* Capabilities */
#define	CCIMX6_HAS_WIRELESS	(1 << 0)
#define	CCIMX6_HAS_BLUETOOTH	(1 << 1)
#define	CCIMX6_HAS_KINETIS	(1 << 2)
#define	CCIMX6_HAS_EMMC		(1 << 3)

#define CONFIG_HWID_STRINGS_HELP	"<high word> <low word>"
#define CONFIG_MANUF_STRINGS_HELP	"<LYYWWGGXXXXXX> <VVHC> <K>"
#define DIGICMD_HWID_SUPPORTED_OPTIONS_HELP \
	     "read - read HWID from shadow registers\n" \
	"hwid read_manuf - read HWID from shadow registers and print manufacturing ID\n" \
	"hwid sense - sense HWID from fuses\n" \
	"hwid sense_manuf - sense HWID from fuses and print manufacturing ID\n" \
	"hwid prog [-y] " CONFIG_HWID_STRINGS_HELP " - program HWID (PERMANENT)\n" \
	"hwid prog_manuf [-y] " CONFIG_MANUF_STRINGS_HELP " - program HWID with manufacturing ID (PERMANENT)\n" \
	"hwid override " CONFIG_HWID_STRINGS_HELP " - override HWID\n" \
	"hwid override_manuf " CONFIG_MANUF_STRINGS_HELP " - override HWID with manufacturing ID\n" \
	"hwid lock [-y] - lock HWID OTP bits (PERMANENT)\n"

#endif	/* __HWID_CC6_H_ */
