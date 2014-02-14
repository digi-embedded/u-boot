/*
 *  Copyright (C) 2014 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

#ifndef __CMD_DIGI_H
#define __CMD_DIGI_H

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

const char *image_source_str[] = {
	"tftp",
	"nfs",
	"flash",
	"usb",
	"mmc",
	"ram",
	"sata",
};


#endif  /* __CMD_DIGI_H */
