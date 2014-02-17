/*
 *  common/helper.h
 *
 *  Copyright (C) 2007 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

/*
 *  !Revision:   $Revision: 1.1 $
 *  !Author:     Markus Pietrek
*/

#ifndef __DIGI_HELPER_H
#define __DIGI_HELPER_H

typedef enum {
	IS_UNDEFINED = -1,
	IS_TFTP,
	IS_NFS,
	IS_FLASH,
	IS_USB,
	IS_MMC,
	IS_RAM,
	IS_SATA,
}image_source_e;

static const char *image_source_str[] = {
	"tftp",
	"nfs",
	"flash",
	"usb",
	"mmc",
	"ram",
	"sata",
};

int confirm_msg(char *msg);
int get_image_source(char *source);
int get_target_partition(char *partname, disk_partition_t *info);
int get_fw_filename(int argc, char * const argv[], image_source_e src,
			   char *filename);
int get_default_filename(char *partname, char *filename);

#endif  /* __DIGI_HELPER_H */
