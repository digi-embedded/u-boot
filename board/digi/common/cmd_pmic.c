/*
 * (C) Copyright 2015-2018
 * Digi International
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <command.h>
#include <mmc.h>

extern int pmic_read_reg(int reg, unsigned char *value);
extern int pmic_write_reg(int reg, unsigned char value);

int do_pmic(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	unsigned int addr;
	unsigned char val;
	int i, j;
	int count = 1;

	if (argc < 2)
		return CMD_RET_USAGE;

	addr = simple_strtol(argv[2], NULL, 16);
	if (strcmp(argv[1], "read") == 0) {
		if (argc < 3)
			return CMD_RET_USAGE;
		if (argc == 4)
			count = simple_strtol(argv[3], NULL, 16);
		for (i = 0; i < count; i++) {
			if (pmic_read_reg(addr + i, &val))
				printf("Error reading PMIC address 0x%x\n",
					addr + i);
			else
				printf("PMIC[0x%x]: 0x%02x\n", addr + i, val);
		}
	} else if (strcmp(argv[1], "write") == 0) {
		if (argc < 4)
			return CMD_RET_USAGE;
		val = simple_strtol(argv[3], NULL, 16);
		if (pmic_write_reg(addr, (unsigned char)val)) {
			printf("Error writing PMIC address 0x%x\n", addr);
			return CMD_RET_FAILURE;
		}
	} else if (strcmp(argv[1], "dump") == 0) {
	       printf("PMIC\t00 01 02 03 04 05 06 07     08 09 0a 0b 0c 0d 0e 0f\n"
		      "    \t---------------------------------------------------\n");
	       for (i = 0, j = 0; i < CONFIG_PMIC_NUMREGS; i++) {
		       if (j == 0)
			       printf("0x%04x:\t", i);
		       if (pmic_read_reg(i, &val))
			       printf("-- ");
		       else
			       printf("%02x ", val);
		       if (j == 7)
			       printf("    ");
		       if (j == 15) {
			       printf("\n");
			       j = 0;
		       } else {
			       j++;
		       }
	       }
	       printf("\n\n");
	} else {
		return CMD_RET_USAGE;
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	pmic, 4, 1, do_pmic,
	"PMIC access",
	"dump - dumps all PMIC registers\n"
	"pmic read address [count] - read PMIC register(s)\n"
	"pmic write address value - write PMIC register"
);
