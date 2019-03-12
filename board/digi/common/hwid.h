/*
 * Copyright 2014-2018 Digi International Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef __HWID_H_
#define __HWID_H_

enum digi_cert {
	DIGI_CERT_USA = 0,
	DIGI_CERT_INTERNATIONAL,
	DIGI_CERT_JAPAN,

	DIGI_MAX_CERT,
};

/* RAM size */
#define MEM_2GB		0x80000000
#define MEM_1GB		0x40000000
#define MEM_512MB	0x20000000
#define MEM_256MB	0x10000000

#ifdef CONFIG_CC6
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
};

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
#endif /* CONFIG_CC6 */

#ifdef CONFIG_CC8
/*
 * HWID is stored in 3 consecutive Fuse Words, being:
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
 *   |     31..16      | 15..11 |  10..6  | 5..3 | 2..0 |
 *   +--------------------------+---------+------+------+
 *   |     RESERVED    |   --   | Variant |  HV  | Cert |
 *   +--------------------------+---------+------+------+
 *
 *              MAC2[31:0] (Bank 0 Word 710)
 *
 *   | 31..28 |  27..24  |             23..0            |
 *   +--------+----------+------------------------------+
 *   |  GenID | MAC pool |            MAC base          |
 *   +--------+----------+------------------------------+
 */
struct __packed digi_hwid {
	u32	sn:20;		/* serial number */
	u32	month:4;	/* manufacturing month */
	u32	year:6;		/* manufacturing year */
	u32	wid:2;		/* wireless ID */
	u32	cert:3;		/* type of wifi certification */
	u32	hv:3;		/* hardware version */
	u32	variant:5;	/* module variant */
	u32	spare:5;	/* spare bits */
	u32	reserved:16;	/* reserved by NXP */
	u32	mac_base:24;	/* MAC base address */
	u32	mac_pool:4;	/* MAC address pool */
	u32	genid:4;	/* generator id */
};

enum imx8_cpu {
	IMX8_NONE = 0,	/* Reserved */
	IMX8QXP,	/* QuadXPlus */
	IMX8DXP,	/* DualXPlus */
};

struct ccimx8_variant {
	enum imx8_cpu	cpu;
	const int	sdram;
	u8		capabilities;
	const char	*id_string;
};

/* Capabilities */
#define	CCIMX8_HAS_WIRELESS	(1 << 0)
#define	CCIMX8_HAS_BLUETOOTH	(1 << 1)

#define CONFIG_HWID_STRINGS_HELP	"<XXXXXXXX> <YYYY> <ZZZZZZZZ>"
#define CONFIG_MANUF_STRINGS_HELP	"<YYMMGGXXXXXX> <PPAAAAAA> <VVHC> <K>"
#endif /* CONFIG_CC8 */

void board_print_hwid(struct digi_hwid *hwid);
void board_print_manufid(struct digi_hwid *hwid);
int board_parse_hwid(int argc, char *const argv[], struct digi_hwid *hwid);
int board_parse_manufid(int argc, char *const argv[], struct digi_hwid *hwid);
int board_read_hwid(struct digi_hwid *hwid);
int board_sense_hwid(struct digi_hwid *hwid);
int board_prog_hwid(const struct digi_hwid *hwid);
int board_override_hwid(const struct digi_hwid *hwid);
int board_lock_hwid(void);
void fdt_fixup_hwid(void *fdt);

#endif	/* __HWID_H_ */
