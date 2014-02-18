/*
 *  Copyright (C) 2014 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

#ifndef __DIGI_HELPER_H
#define __DIGI_HELPER_H

enum {
	SRC_UNDEFINED = -1,
	SRC_TFTP,
	SRC_NFS,
	SRC_FLASH,
	SRC_USB,
	SRC_MMC,
	SRC_RAM,
	SRC_SATA,
};

int confirm_msg(char *msg);
int get_source(char *src);
int get_target_partition(char *partname, disk_partition_t *info);
int get_fw_filename(int argc, char * const argv[], int src, char *filename);
int get_default_filename(char *partname, char *filename);
int load_firmware_to_ram(int src, char *filename, char *devpartno,
			 char *fs, char *loadaddr, char *varload);

#endif  /* __DIGI_HELPER_H */
