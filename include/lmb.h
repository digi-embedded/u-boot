/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _LINUX_LMB_H
#define _LINUX_LMB_H
#ifdef __KERNEL__

#include <asm/types.h>
#include <asm/u-boot.h>

/*
 * Logical memory blocks.
 *
 * Copyright (C) 2001 Peter Bergner, IBM Corp.
 */

/**
 * enum lmb_flags - definition of memory region attributes
 * @LMB_NONE: no special request
 * @LMB_NOMAP: don't add to mmu configuration
 */
enum lmb_flags {
	LMB_NONE		= 0x0,	/* No special request */
	LMB_NOMAP		= 0x4,	/* don't add to mmu config */
};

struct lmb_property {
	phys_addr_t base;
	phys_size_t size;
	enum lmb_flags flags;
};

struct lmb_region {
	unsigned long cnt;	/* number of regions */
	unsigned long max;	/* size of the allocated array */
	struct lmb_property *region;
};

struct lmb {
	struct lmb_region memory;
	struct lmb_region reserved;
	struct lmb_property memory_regions[CONFIG_LMB_MEMORY_REGIONS + 1];
	struct lmb_property reserved_regions[CONFIG_LMB_RESERVED_REGIONS + 1];
};

extern void lmb_init(struct lmb *lmb);
extern void lmb_init_and_reserve(struct lmb *lmb, struct bd_info *bd,
				 void *fdt_blob);
extern void lmb_init_and_reserve_range(struct lmb *lmb, phys_addr_t base,
				       phys_size_t size, void *fdt_blob);
extern long lmb_add(struct lmb *lmb, phys_addr_t base, phys_size_t size);
extern long lmb_reserve(struct lmb *lmb, phys_addr_t base, phys_size_t size);
extern long lmb_reserve_flags(struct lmb *lmb, phys_addr_t base,
			      phys_size_t size, enum lmb_flags flags);
extern phys_addr_t lmb_alloc(struct lmb *lmb, phys_size_t size, ulong align);
extern phys_addr_t lmb_alloc_base(struct lmb *lmb, phys_size_t size, ulong align,
			    phys_addr_t max_addr);
extern phys_addr_t __lmb_alloc_base(struct lmb *lmb, phys_size_t size, ulong align,
			      phys_addr_t max_addr);
extern phys_addr_t lmb_alloc_addr(struct lmb *lmb, phys_addr_t base,
				  phys_size_t size);
extern phys_size_t lmb_get_free_size(struct lmb *lmb, phys_addr_t addr);
extern int lmb_is_reserved(struct lmb *lmb, phys_addr_t addr);
extern int lmb_is_reserved_flags(struct lmb *lmb, phys_addr_t addr, int flags);
extern long lmb_free(struct lmb *lmb, phys_addr_t base, phys_size_t size);

extern void lmb_dump_all(struct lmb *lmb);
extern void lmb_dump_all_force(struct lmb *lmb);

static inline phys_size_t
lmb_size_bytes(struct lmb_region *type, unsigned long region_nr)
{
	return type->region[region_nr].size;
}

void board_lmb_reserve(struct lmb *lmb);
void arch_lmb_reserve(struct lmb *lmb);

/* Low level functions */

static inline bool lmb_is_nomap(struct lmb_property *m)
{
	return !!(m->flags & LMB_NOMAP);
}

#endif /* __KERNEL__ */

#endif /* _LINUX_LMB_H */
