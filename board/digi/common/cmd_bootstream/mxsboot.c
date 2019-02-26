/*
 * Freescale i.MX28 image generator
 *
 * Copyright (C) 2011 Marek Vasut <marek.vasut@gmail.com>
 * on behalf of DENX Software Engineering GmbH
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <nand.h>

/*
 * Default BCB layout.
 *
 * TWEAK this if you have blown any OCOTP fuses.
 */
#define	STRIDE_PAGES		64
#define	STRIDE_COUNT		4

/*
 * Layout for 256Mb big NAND with 2048b page size, 64b OOB size and
 * 128kb erase size.
 *
 * TWEAK this if you have different kind of NAND chip.
 */
uint32_t nand_writesize = 2048;
uint32_t nand_oobsize = 64;
uint32_t nand_erasesize = 128 * 1024;

/*
 * Sector on which the SigmaTel boot partition (0x53) starts.
 */
uint32_t sd_sector = 2048;

/*
 * Each of the U-Boot bootstreams is at maximum 1MB big.
 *
 * TWEAK this if, for some wild reason, you need to boot bigger image.
 */
#define	MAX_BOOTSTREAM_SIZE	(1 * 1024 * 1024)

/* i.MX28 NAND controller-specific constants. DO NOT TWEAK! */
#define	MXS_NAND_DMA_DESCRIPTOR_COUNT		4
#define	MXS_NAND_CHUNK_DATA_CHUNK_SIZE		512
#define	MXS_NAND_METADATA_SIZE			10
#define	MXS_NAND_COMMAND_BUFFER_SIZE		32

struct mx28_nand_fcb {
	uint32_t		checksum;
	uint32_t		fingerprint;
	uint32_t		version;
	struct {
		uint8_t			data_setup;
		uint8_t			data_hold;
		uint8_t			address_setup;
		uint8_t			dsample_time;
		uint8_t			nand_timing_state;
		uint8_t			rea;
		uint8_t			rloh;
		uint8_t			rhoh;
	}			timing;
	uint32_t		page_data_size;
	uint32_t		total_page_size;
	uint32_t		sectors_per_block;
	uint32_t		number_of_nands;		/* Ignored */
	uint32_t		total_internal_die;		/* Ignored */
	uint32_t		cell_type;			/* Ignored */
	uint32_t		ecc_block_n_ecc_type;
	uint32_t		ecc_block_0_size;
	uint32_t		ecc_block_n_size;
	uint32_t		ecc_block_0_ecc_type;
	uint32_t		metadata_bytes;
	uint32_t		num_ecc_blocks_per_page;
	uint32_t		ecc_block_n_ecc_level_sdk;	/* Ignored */
	uint32_t		ecc_block_0_size_sdk;		/* Ignored */
	uint32_t		ecc_block_n_size_sdk;		/* Ignored */
	uint32_t		ecc_block_0_ecc_level_sdk;	/* Ignored */
	uint32_t		num_ecc_blocks_per_page_sdk;	/* Ignored */
	uint32_t		metadata_bytes_sdk;		/* Ignored */
	uint32_t		erase_threshold;
	uint32_t		boot_patch;
	uint32_t		patch_sectors;
	uint32_t		firmware1_starting_sector;
	uint32_t		firmware2_starting_sector;
	uint32_t		sectors_in_firmware1;
	uint32_t		sectors_in_firmware2;
	uint32_t		dbbt_search_area_start_address;
	uint32_t		badblock_marker_byte;
	uint32_t		badblock_marker_start_bit;
	uint32_t		bb_marker_physical_offset;
};

struct mx28_nand_dbbt {
	uint32_t		checksum;
	uint32_t		fingerprint;
	uint32_t		version;
	uint32_t		number_bb;
	uint32_t		number_2k_pages_bb;
};

struct mx28_nand_bbt {
	uint32_t		nand;
	uint32_t		number_bb;
	uint32_t		badblock[510];
};

struct mx28_sd_drive_info {
	uint32_t		chip_num;
	uint32_t		drive_type;
	uint32_t		tag;
	uint32_t		first_sector_number;
	uint32_t		sector_count;
};

struct mx28_sd_config_block {
	uint32_t			signature;
	uint32_t			primary_boot_tag;
	uint32_t			secondary_boot_tag;
	uint32_t			num_copies;
	struct mx28_sd_drive_info	drv_info[1];
};

static inline uint32_t mx28_nand_ecc_size_in_bits(uint32_t ecc_strength)
{
	return ecc_strength * 13;
}

static inline uint32_t mx28_nand_get_ecc_strength(uint32_t page_data_size,
						uint32_t page_oob_size)
{
	struct mtd_info *mtd = get_nand_dev_by_index(0);
	struct nand_chip *chip = mtd_to_nand(mtd);

	return round_up(chip->ecc_strength_ds, 2);
}

static inline uint32_t mx28_nand_get_mark_offset(uint32_t page_data_size,
						uint32_t ecc_strength)
{
	uint32_t chunk_data_size_in_bits;
	uint32_t chunk_ecc_size_in_bits;
	uint32_t chunk_total_size_in_bits;
	uint32_t block_mark_chunk_number;
	uint32_t block_mark_chunk_bit_offset;
	uint32_t block_mark_bit_offset;

	chunk_data_size_in_bits = MXS_NAND_CHUNK_DATA_CHUNK_SIZE * 8;
	chunk_ecc_size_in_bits  = mx28_nand_ecc_size_in_bits(ecc_strength);

	chunk_total_size_in_bits =
			chunk_data_size_in_bits + chunk_ecc_size_in_bits;

	/* Compute the bit offset of the block mark within the physical page. */
	block_mark_bit_offset = page_data_size * 8;

	/* Subtract the metadata bits. */
	block_mark_bit_offset -= MXS_NAND_METADATA_SIZE * 8;

	/*
	 * Compute the chunk number (starting at zero) in which the block mark
	 * appears.
	 */
	block_mark_chunk_number =
			block_mark_bit_offset / chunk_total_size_in_bits;

	/*
	 * Compute the bit offset of the block mark within its chunk, and
	 * validate it.
	 */
	block_mark_chunk_bit_offset = block_mark_bit_offset -
			(block_mark_chunk_number * chunk_total_size_in_bits);

	if (block_mark_chunk_bit_offset > chunk_data_size_in_bits)
		return 1;

	/*
	 * Now that we know the chunk number in which the block mark appears,
	 * we can subtract all the ECC bits that appear before it.
	 */
	block_mark_bit_offset -=
		block_mark_chunk_number * chunk_ecc_size_in_bits;

	return block_mark_bit_offset;
}

uint32_t mx28_nand_mark_byte_offset(void)
{
	uint32_t ecc_strength;
	ecc_strength = mx28_nand_get_ecc_strength(nand_writesize, nand_oobsize);
	return mx28_nand_get_mark_offset(nand_writesize, ecc_strength) >> 3;
}

uint32_t mx28_nand_mark_bit_offset(void)
{
	uint32_t ecc_strength;
	ecc_strength = mx28_nand_get_ecc_strength(nand_writesize, nand_oobsize);
	return mx28_nand_get_mark_offset(nand_writesize, ecc_strength) & 0x7;
}
