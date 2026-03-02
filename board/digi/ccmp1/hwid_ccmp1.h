/*
 * Copyright 2022 Digi International Inc
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __HWID_CCMP1_H_
#define __HWID_CCMP1_H_

/*
 * On the CCMP1 the HWID is stored in 3 consecutive Fuse Words, being:
 *
 *                    XK27[31..0]  (bank 0 word 59)
 *
 *   |  31..26  |  25..20  |            19..0           |
 *   +----------+----------+----------------------------+
 *   |   Year   |   Week   |        Serial number       |
 *   +----------+----------+----------------------------+
 *
 *                    XK28[31..0]  (bank 0 word 60)
 *
 *   | 31..28 |  27..24  |             23..0            |
 *   +--------+----------+------------------------------+
 *   |  GenID | MAC pool |            MAC base          |
 *   +--------+----------+------------------------------+
 *
 *                    XK29[31..0]  (bank 0 word 61)
 *
 *   | 31..20 |   19   | 18 |   17  |  16 |15..12|  11..7  |6..3| 2..0 |
 *   +--------+--------+----+-------+-----+------+---------+----+------+
 *   |   --   | Crypto | BT | Wi-Fi | MCA |  RAM | Variant | HV | Cert |
 *   +--------+--------+----+-------+-----+------+---------+----+------+
 */
struct __packed digi_hwid {
	/* Word 0 */
 	u32	sn:20;		/* serial number */
	u32	week:6;		/* manufacturing week */
	u32	year:6;		/* manufacturing year */
	/* Word 1 */
	u32	mac_base:24;	/* MAC base address */
	u32	mac_pool:4;	/* MAC address pool */
	u32	genid:4;	/* generator id */
	/* Word 2 */
	u32	cert:3;		/* type of wifi certification */
	u32	hv:4;		/* hardware version */
	u32	variant:5;	/* module variant */
	u32	ram:4;		/* RAM */
	u32	mca:1;		/* has MCA */
	u32	wifi:1;		/* has Wi-Fi */
	u32	bt:1;		/* has Bluetooth */
	u32	crypto:1;	/* has crypto-authentication */
	u32	spare:12;	/* spare */
}__aligned(4);

#define CONFIG_HWID_STRINGS_HELP	"<XXXXXXXX> <YYYYYYYY> <ZZZZZZZZ>"
#define CONFIG_MANUF_STRINGS_HELP	"<YYWWGGXXXXXX> <PPAAAAAA> <VVHC> <RMWBC>"

#endif /* __HWID_CCMP1_H_ */
