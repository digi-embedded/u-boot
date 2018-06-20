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

struct digi_hwid {
	unsigned char	location;	/* location */
	u8		variant;	/* module variant */
	unsigned char	hv;		/* hardware version */
	unsigned char	cert;		/* type of wifi certification */
	u8		year;		/* manufacturing year */
	u8		week;		/* manufacturing week */
	u8		genid;		/* generator id */
	u32		sn;		/* serial number */
	unsigned char	wid;		/* wireless ID */
};

#define HWID_ARRAY_WORDS_NUMBER    2

enum digi_cert{
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

#define CONFIG_MANUF_STRINGS_HELP	"<LYYWWGGXXXXXX> <VVHC> <K>"

#ifdef CONFIG_CC6
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
#endif /* CONFIG_CC6 */

#ifdef CONFIG_CC8

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
#endif /* CONFIG_CC8 */

int board_read_hwid(u32 *hwid_array);
int board_sense_hwid(u32 *hwid_array);
int board_prog_hwid(const u32 *hwid_array);
int board_override_hwid(const u32 *hwid_array);
int board_lock_hwid(void);
int board_get_hwid(struct digi_hwid *hwid);
void fdt_fixup_hwid(void *fdt);
void board_fdt_fixup_hwid(void *fdt, struct digi_hwid *hwid);

#endif	/* __HWID_H_ */
