// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000-2010
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * (C) Copyright 2008
 * Stuart Wood, Lab X Technologies <stuart.wood@labxtechnologies.com>
 *
 * (C) Copyright 2004
 * Jian Zhang, Texas Instruments, jzhang@ti.com.
 *
 * (C) Copyright 2001 Sysgo Real-Time Solutions, GmbH <www.elinos.com>
 * Andreas Heppel <aheppel@sysgo.de>
 */

#include <common.h>
#include <command.h>
#include <env.h>
#include <env_internal.h>
#include <linux/stddef.h>
#include <malloc.h>
#include <memalign.h>
#include <nand.h>
#include <search.h>
#include <errno.h>
#include <u-boot/crc.h>

#if defined(CONFIG_CMD_SAVEENV) && defined(CONFIG_CMD_NAND) && \
		!defined(CONFIG_SPL_BUILD)
#define CMD_SAVEENV
#elif defined(CONFIG_ENV_OFFSET_REDUND) && !defined(CONFIG_SPL_BUILD)
#error CONFIG_ENV_OFFSET_REDUND must have CONFIG_CMD_SAVEENV & CONFIG_CMD_NAND
#endif

#ifndef CONFIG_ENV_RANGE
#define CONFIG_ENV_RANGE	CONFIG_ENV_SIZE
#endif

#if defined(ENV_IS_EMBEDDED)
static env_t *env_ptr = &environment;
#elif defined(CONFIG_NAND_ENV_DST)
static env_t *env_ptr = (env_t *)CONFIG_NAND_ENV_DST;
#endif /* ENV_IS_EMBEDDED */

DECLARE_GLOBAL_DATA_PTR;

#ifdef OLD_ENV_OFFSET_LOCATIONS
extern void generate_partition_table(void);
long long env_get_offset_old(int index);
#endif

struct nand_env_location {
	const char *name;
	nand_erase_options_t erase_opts;
};

static struct nand_env_location location[] = {
	{
		.name = "NAND",
		.erase_opts = {
			.length = CONFIG_ENV_RANGE,
			.offset = CONFIG_ENV_OFFSET,
		},
	},
#ifdef CONFIG_ENV_OFFSET_REDUND
	{
		.name = "redundant NAND",
		.erase_opts = {
			.length = CONFIG_ENV_RANGE,
			.offset = CONFIG_ENV_OFFSET_REDUND,
		},
	},
#endif
};

#ifdef CONFIG_DYNAMIC_ENV_LOCATION
/*
 * Dynamically locate the environment and its redundant copy (if available)
 * in the first available good sectors in the area starting at CONFIG_ENV_OFFSET
 * and with a range defined by CONFIG_ENV_RANGE.
 */
static void env_set_dynamic_location(struct nand_env_location *location,
				     int old_env)
{
	loff_t off;
	int i = 0;
	int env_copies = 1;
	struct mtd_info *mtd;
	loff_t env_offset;
	loff_t env_first_noenv_sector;

	/* Determine offset of env partition depending on NAND size */
	if (old_env)
		env_offset = env_get_offset_old(old_env);
	else
		env_offset = env_get_offset(CONFIG_ENV_OFFSET);
	env_first_noenv_sector = env_offset + CONFIG_ENV_RANGE;

	mtd = get_nand_dev_by_index(0);
	if (!mtd)
		return;

	if (CONFIG_ENV_SIZE > mtd->erasesize)
		printf("Warning: environment size larger than PEB size is not supported\n");

	/* Init env offsets */
	location[0].erase_opts.offset = env_offset;

#ifdef CONFIG_ENV_OFFSET_REDUND
	env_copies++;

	/* Init redundant copy offset */
	location[1].erase_opts.offset = env_offset + mtd->erasesize;
#endif

	/*
	 * Relocate (if needed) each copy on the first good block that is not
	 * used by the other copy.
	 */
	for (i = 0; i < env_copies; i++) {
		/* limit erase size to one erase block */
		location[i].erase_opts.length = mtd->erasesize;

		for (off = env_offset;
		     off < env_first_noenv_sector;
		     off += mtd->erasesize) {
			if (!nand_block_isbad(mtd, off)) {
				if (off == location[i].erase_opts.offset) {
					/* already set in a good block */
					break;
				}
#ifdef CONFIG_ENV_OFFSET_REDUND
				if (off == location[!i].erase_opts.offset) {
					/* skip block where the other copy is */
					continue;
				}
#endif
				/* assign good block to work on */
				location[i].erase_opts.offset = off;
				break;
			}
		}

		if (off >= env_first_noenv_sector)
			printf("Warning: no available good sectors for %s environment\n",
			       i ? "redundant" : "primary");
		else
			debug("env[%i].offset=%llx\n", i, off);
	}
}
#endif /* CONFIG_DYNAMIC_ENV_LOCATION */

/*
 * This is called before nand_init() so we can't read NAND to
 * validate env data.
 *
 * Mark it OK for now. env_relocate() in env_common.c will call our
 * relocate function which does the real validation.
 *
 * When using a NAND boot image (like sequoia_nand), the environment
 * can be embedded or attached to the U-Boot image in NAND flash.
 * This way the SPL loads not only the U-Boot image from NAND but
 * also the environment.
 */
static int env_nand_init(void)
{
#if defined(ENV_IS_EMBEDDED) || defined(CONFIG_NAND_ENV_DST)
	int crc1_ok = 0, crc2_ok = 0;
	env_t *tmp_env1;

#ifdef CONFIG_ENV_OFFSET_REDUND
	env_t *tmp_env2;

	tmp_env2 = (env_t *)((ulong)env_ptr + CONFIG_ENV_SIZE);
	crc2_ok = crc32(0, tmp_env2->data, ENV_SIZE) == tmp_env2->crc;
#endif
	tmp_env1 = env_ptr;
	crc1_ok = crc32(0, tmp_env1->data, ENV_SIZE) == tmp_env1->crc;

	if (!crc1_ok && !crc2_ok) {
		gd->env_addr	= 0;
		gd->env_valid	= ENV_INVALID;

		return 0;
	} else if (crc1_ok && !crc2_ok) {
		gd->env_valid = ENV_VALID;
	}
#ifdef CONFIG_ENV_OFFSET_REDUND
	else if (!crc1_ok && crc2_ok) {
		gd->env_valid = ENV_REDUND;
	} else {
		/* both ok - check serial */
		if (tmp_env1->flags == 255 && tmp_env2->flags == 0)
			gd->env_valid = ENV_REDUND;
		else if (tmp_env2->flags == 255 && tmp_env1->flags == 0)
			gd->env_valid = ENV_VALID;
		else if (tmp_env1->flags > tmp_env2->flags)
			gd->env_valid = ENV_VALID;
		else if (tmp_env2->flags > tmp_env1->flags)
			gd->env_valid = ENV_REDUND;
		else /* flags are equal - almost impossible */
			gd->env_valid = ENV_VALID;
	}

	if (gd->env_valid == ENV_REDUND)
		env_ptr = tmp_env2;
	else
#endif
	if (gd->env_valid == ENV_VALID)
		env_ptr = tmp_env1;

	gd->env_addr = (ulong)env_ptr->data;

#else /* ENV_IS_EMBEDDED || CONFIG_NAND_ENV_DST */
	gd->env_addr	= (ulong)&default_environment[0];
	gd->env_valid	= ENV_VALID;
#endif /* ENV_IS_EMBEDDED || CONFIG_NAND_ENV_DST */

	return 0;
}

#ifdef CMD_SAVEENV
/*
 * The legacy NAND code saved the environment in the first NAND device i.e.,
 * nand_dev_desc + 0. This is also the behaviour using the new NAND code.
 */
static int writeenv(size_t offset, u_char *buf)
{
	size_t end = offset + CONFIG_ENV_RANGE;
	size_t amount_saved = 0;
	size_t blocksize, len;
	struct mtd_info *mtd;
	u_char *char_ptr;

	mtd = get_nand_dev_by_index(0);
	if (!mtd)
		return 1;

#ifdef CONFIG_DYNAMIC_ENV_LOCATION
	end = offset + mtd->erasesize;
#endif

	blocksize = mtd->erasesize;
	len = min(blocksize, (size_t)CONFIG_ENV_SIZE);

	while (amount_saved < CONFIG_ENV_SIZE && offset < end) {
		if (nand_block_isbad(mtd, offset)) {
			offset += blocksize;
		} else {
			char_ptr = &buf[amount_saved];
			if (nand_write(mtd, offset, &len, char_ptr))
				return 1;

			offset += blocksize;
			amount_saved += len;
		}
	}
	if (amount_saved != CONFIG_ENV_SIZE)
		return 1;

	return 0;
}

static int erase_and_write_env(const struct nand_env_location *location,
		u_char *env_new)
{
	struct mtd_info *mtd;
	int ret = 0;

	mtd = get_nand_dev_by_index(0);
	if (!mtd)
		return 1;

	printf("Erasing %s...\n", location->name);
	if (nand_erase_opts(mtd, &location->erase_opts))
		return 1;

	printf("Writing to %s... ", location->name);
	ret = writeenv(location->erase_opts.offset, env_new);
	puts(ret ? "FAILED!\n" : "OK\n");

	return ret;
}

static int env_nand_save(void)
{
	int	ret = 0;
	ALLOC_CACHE_ALIGN_BUFFER(env_t, env_new, 1);
	int	env_idx = 0;

	if (CONFIG_ENV_RANGE < CONFIG_ENV_SIZE)
		return 1;

	ret = env_export(env_new);
	if (ret)
		return ret;

#ifdef CONFIG_ENV_OFFSET_REDUND
	env_idx = (gd->env_valid == ENV_VALID);
#endif

#ifdef CONFIG_DYNAMIC_ENV_LOCATION
	env_set_dynamic_location(location, 0);
#endif

	ret = erase_and_write_env(&location[env_idx], (u_char *)env_new);
#ifdef CONFIG_ENV_OFFSET_REDUND
	if (!ret) {
		/* preset other copy for next write */
		gd->env_valid = gd->env_valid == ENV_REDUND ? ENV_VALID :
				ENV_REDUND;
		return ret;
	}

	env_idx = (env_idx + 1) & 1;
	ret = erase_and_write_env(&location[env_idx], (u_char *)env_new);
	if (!ret)
		printf("Warning: primary env write failed,"
				" redundancy is lost!\n");
#endif

	return ret;
}
#endif /* CMD_SAVEENV */

#if defined(CONFIG_SPL_BUILD)
static int readenv(size_t offset, u_char *buf)
{
	return nand_spl_load_image(offset, CONFIG_ENV_SIZE, buf);
}
#else
static int readenv(size_t offset, u_char *buf)
{
	size_t end = offset + CONFIG_ENV_RANGE;
	size_t amount_loaded = 0;
	size_t blocksize, len;
	struct mtd_info *mtd;
	u_char *char_ptr;

	mtd = get_nand_dev_by_index(0);
	if (!mtd)
		return 1;

	blocksize = mtd->erasesize;
	len = min(blocksize, (size_t)CONFIG_ENV_SIZE);

	while (amount_loaded < CONFIG_ENV_SIZE && offset < end) {
		if (nand_block_isbad(mtd, offset)) {
			offset += blocksize;
		} else {
			char_ptr = &buf[amount_loaded];
			if (nand_read_skip_bad(mtd, offset,
					       &len, NULL,
					       mtd->size, char_ptr))
				return 1;

			offset += blocksize;
			amount_loaded += len;
		}
	}

	if (amount_loaded != CONFIG_ENV_SIZE)
		return 1;

	return 0;
}
#endif /* #if defined(CONFIG_SPL_BUILD) */

#ifdef CONFIG_ENV_OFFSET_OOB
int get_nand_env_oob(struct mtd_info *mtd, unsigned long *result)
{
	struct mtd_oob_ops ops;
	uint32_t oob_buf[ENV_OFFSET_SIZE / sizeof(uint32_t)];
	int ret;

	ops.datbuf	= NULL;
	ops.mode	= MTD_OOB_AUTO;
	ops.ooboffs	= 0;
	ops.ooblen	= ENV_OFFSET_SIZE;
	ops.oobbuf	= (void *)oob_buf;

	ret = mtd->read_oob(mtd, ENV_OFFSET_SIZE, &ops);
	if (ret) {
		printf("error reading OOB block 0\n");
		return ret;
	}

	if (oob_buf[0] == ENV_OOB_MARKER) {
		*result = ovoid ob_buf[1] * mtd->erasesize;
	} else if (oob_buf[0] == ENV_OOB_MARKER_OLD) {
		*result = oob_buf[1];
	} else {
		printf("No dynamic environment marker in OOB block 0\n");
		return -ENOENT;
	}

	return 0;
}
#endif

#ifdef CONFIG_ENV_OFFSET_REDUND
static int env_nand_load(void)
{
#if defined(ENV_IS_EMBEDDED)
	return 0;
#else
	int read1_fail, read2_fail;
	env_t *tmp_env1, *tmp_env2;
	int ret = 0;

	tmp_env1 = (env_t *)malloc(CONFIG_ENV_SIZE);
	tmp_env2 = (env_t *)malloc(CONFIG_ENV_SIZE);
	if (tmp_env1 == NULL || tmp_env2 == NULL) {
		puts("Can't allocate buffers for environment\n");
		env_set_default("malloc() failed", 0);
		ret = -EIO;
		goto done;
	}

#ifdef CONFIG_DYNAMIC_ENV_LOCATION
	env_set_dynamic_location(location, 0);

	read1_fail = readenv(location[0].erase_opts.offset,
			     (u_char *)tmp_env1);
	read2_fail = readenv(location[1].erase_opts.offset,
			     (u_char *)tmp_env2);
#else
	read1_fail = readenv(env_get_offset(CONFIG_ENV_OFFSET),
			     (u_char *)tmp_env1);
	read2_fail = readenv(env_get_offset_redund(CONFIG_ENV_OFFSET_REDUND),
			     (u_char *)tmp_env2);
#endif
	ret = env_import_redund((char *)tmp_env1, read1_fail, (char *)tmp_env2,
				read2_fail);
#if defined(OLD_ENV_OFFSET_LOCATIONS) && defined(CONFIG_DYNAMIC_ENV_LOCATION)
	if (ret) {
		/*
		 * If a valid environment can't be read, try to recover it from
		 * the old offset. This mechanism helps when we need to move
		 * the offset of the environment, between versions of U-Boot.
		 */
		struct nand_env_location old_location[2];
		int old_env = 1;
		char mtdparts_old[256];
		char mtdparts_new[256];
		char *var;

		do {
			/* Init struct as a copy from the current one */
			memcpy(old_location, location, sizeof(location));

			/* Set offsets to old location */
			env_set_dynamic_location(old_location, old_env);

			/* Read from old environment offset */
			debug("Checking env at offset 0x%llx (%d)\n",
			      old_location[0].erase_opts.offset, old_env);
			read1_fail = readenv(old_location[0].erase_opts.offset,
					(u_char *)tmp_env1);
			read2_fail = readenv(old_location[1].erase_opts.offset,
					(u_char *)tmp_env2);
			ret = env_import_redund((char *)tmp_env1, read1_fail,
						(char *)tmp_env2, read2_fail);
		} while (ret && old_env++ < OLD_ENV_OFFSET_LOCATIONS);

		if (ret)
			goto done;

		/* Save environment after restoration in its proper place */
		printf("Restoring environment from previous location\n");

		/*
		 * Restored environment may have a different partition table
		 * than the one pre-compiled in this U-Boot, so re-generate it
		 * and warn if there are changes.
		 */
		var = env_get("mtdparts");
		strcpy(mtdparts_old, var);
		generate_partition_table();
		var = env_get("mtdparts");
		strcpy(mtdparts_new, var);
		if (strcmp(mtdparts_old, mtdparts_new)) {
			printf("   WARNING: There is a new prebuilt partition table.\n" \
			       "            Old partition table was not restored.\n" \
			       "            Old: %s\n" \
			       "            New: %s\n",
			       mtdparts_old, mtdparts_new);
		}

		env_nand_save();
#ifdef CONFIG_ENV_OFFSET_REDUND
		/* The environment moved so we want to save twice */
		env_nand_save();
#endif
	}
#endif
done:
	free(tmp_env1);
	free(tmp_env2);

	return ret;
#endif /* ! ENV_IS_EMBEDDED */
}
#else /* ! CONFIG_ENV_OFFSET_REDUND */
/*
 * The legacy NAND code saved the environment in the first NAND
 * device i.e., nand_dev_desc + 0. This is also the behaviour using
 * the new NAND code.
 */
static int env_nand_load(void)
{
#if !defined(ENV_IS_EMBEDDED)
	int ret;
	ALLOC_CACHE_ALIGN_BUFFER(char, buf, CONFIG_ENV_SIZE);

#if defined(CONFIG_ENV_OFFSET_OOB)
	struct mtd_info *mtd  = get_nand_dev_by_index(0);
	/*
	 * If unable to read environment offset from NAND OOB then fall through
	 * to the normal environment reading code below
	 */
	if (mtd && !get_nand_env_oob(mtd, &nand_env_oob_offset)) {
		printf("Found Environment offset in OOB..\n");
	} else {
		env_set_default("no env offset in OOB", 0);
		return;
	}
#endif

#ifdef CONFIG_DYNAMIC_ENV_LOCATION
	env_set_dynamic_location(location);

	ret = readenv(location[0].erase_opts.offset, (u_char *)buf);
#else
	ret = readenv(env_get_offset(CONFIG_ENV_OFFSET), (u_char *)buf);
#endif
	if (ret) {
		env_set_default("readenv() failed", 0);
		return -EIO;
	}

	return env_import(buf, 1);
#endif /* ! ENV_IS_EMBEDDED */

	return 0;
}
#endif /* CONFIG_ENV_OFFSET_REDUND */

U_BOOT_ENV_LOCATION(nand) = {
	.location	= ENVL_NAND,
	ENV_NAME("NAND")
	.load		= env_nand_load,
#if defined(CMD_SAVEENV)
	.save		= env_save_ptr(env_nand_save),
#endif
	.init		= env_nand_init,
};
