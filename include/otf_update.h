/*
 *  Copyright (C) 2014 by Digi International Inc.
 *  All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License version2  as published by
 *  the Free Software Foundation.
*/

#ifndef __OTF_UPDATE_H
#define __OTF_UPDATE_H

/* OTF flags */
#define OTF_FLAG_FLUSH 		(1 << 0)	/* flag to write last chunk */
#define OTF_FLAG_INIT 		(1 << 1)	/* flag to write first chunk */

typedef struct otf_data {
	void *loadaddr;		/* address in RAM to load data to */
	unsigned int offset;	/* offset in media to write data to */
	unsigned char *buf;	/* buffer with data to write */
	unsigned int len;	/* length of chunk to write */
	disk_partition_t *part;	/* partition data */
	unsigned int flags;	/* on-the-fly flags */
}otf_data_t;

#endif  /* __OTF_UPDATE_H */
