/*
 * Copyright 2019 Digi International Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#ifndef __HWID_CC8M_H_
#define __HWID_CC8M_H_

/*
 * On the CC8M the HWID is stored in 3 consecutive Fuse Words, being:
 *
 *                    MAC_ADDR0[31..0]  (bank 9 word 0)
 *
 *   | 31..30 | 29..24 | 23..20 |         19..0         |
 *   +--------+--------+--------+-----------------------+
 *   |   WID  |  Year  | Month  |     Serial number     |
 *   +--------+--------+--------+-----------------------+
 *
 *                    MAC_ADDR1[31..0]  (bank 9 word 1)
 *
 *   | 31..28 |  27..24  |             23..0            |
 *   +--------+----------+------------------------------+
 *   |  GenID | MAC pool |            MAC base          |
 *   +--------+----------+------------------------------+
 *
 *                    MAC_ADDR2[31..0]  (bank 9 word 2)
 *
 *   | 31..25 | 24..19 |   18   | 17 |   16  |  15 |14..11|  10..6  |5..3| 2..0 |
 *   +--------+--------+--------+----+-------+-----+------+---------+----+------+
 *   |   --   |  Week  | Crypto | BT | Wi-Fi | MCA |  RAM | Variant | HV | Cert |
 *   +--------+--------+--------+----+-------+-----+------+---------+----+------+
 */

 struct __packed digi_hwid {
 	u32	sn:20;		/* serial number */
	u32	month:4;	/* manufacturing month */
	u32	year:6;		/* manufacturing year */
	u32	wid:2;		/* wireless ID */
	u32	mac_base:24;	/* MAC base address */
	u32	mac_pool:4;	/* MAC address pool */
	u32	genid:4;	/* generator id */
	u32	cert:3;		/* type of wifi certification */
	u32	hv:3;		/* hardware version */
	u32	variant:5;	/* module variant */
	u32	ram:4;		/* RAM */
	u32	mca:1;		/* has MCA */
	u32	wifi:1;		/* has Wi-Fi */
	u32	bt:1;		/* has Bluetooth */
	u32	crypto:1;	/* has crypto-authentication */
	u32	week:6;		/* manufacturing week */
	u32	spare:7;	/* spare */
}__aligned(4);

#define CONFIG_HWID_STRINGS_HELP	"<XXXXXXXX> <YYYYYYYY> <ZZZZZZZZ>"
#define CONFIG_MANUF_STRINGS_HELP	"<YYWWGGXXXXXX> <PPAAAAAA> <VVHC> <K> <RMWBC>"
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

#endif	/* __HWID_CC8M_H_ */
