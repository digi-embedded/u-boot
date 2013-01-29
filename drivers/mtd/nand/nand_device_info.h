/*
 * Copyright 2009 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */
#ifndef __DRIVERS_NAND_DEVICE_INFO_H
#define __DRIVERS_NAND_DEVICE_INFO_H

 /*
  * The number of ID bytes to read from the NAND Flash device and hand over to
  * the identification system.
  */

#define NAND_DEVICE_ID_BYTE_COUNT  (6)

#define bool int
#define false 0
#define true  1

 /*
  * The number of ID bytes to read from the NAND Flash device and hand over to
  * the identification system.
  */

enum nand_device_cell_technology {
	NAND_DEVICE_CELL_TECH_SLC = 0,
	NAND_DEVICE_CELL_TECH_MLC = 1,
};

/**
 * struct nand_device_info - Information about a single NAND Flash device.
 *
 * This structure contains all the *essential* information about a NAND Flash
 * device, derived from the device's data sheet. For each manufacturer, we have
 * an array of these structures.
 *
 * @end_of_table:              If true, marks the end of a table of device
 *                             information.
 * @manufacturer_code:         The manufacturer code (1st ID byte) reported by
 *                             the device.
 * @device_code:               The device code (2nd ID byte) reported by the
 *                             device.
 * @cell_technology:           The storage cell technology.
 * @chip_size_in_bytes:        The total size of the storage behind a single
 *                             chip select, in bytes. Notice that this is *not*
 *                             necessarily the total size of the storage in a
 *                             *package*, which may contain several chips.
 * @block_size_in_pages:       The number of pages in a block.
 * @page_total_size_in_bytes:  The total size of a page, in bytes, including
 *                             both the data and the OOB.
 * @ecc_strength_in_bits:      The strength of the ECC called for by the
 *                             manufacturer, in number of correctable bits.
 * @ecc_size_in_bytes:         The size of the data block over which the
 *                             manufacturer calls for the given ECC algorithm
 *                             and strength.
 * @data_setup_in_ns:          The data setup time, in nanoseconds. Usually the
 *                             maximum of tDS and tWP. A negative value
 *                             indicates this characteristic isn't known.
 * @data_hold_in_ns:           The data hold time, in nanoseconds. Usually the
 *                             maximum of tDH, tWH and tREH. A negative value
 *                             indicates this characteristic isn't known.
 * @address_setup_in_ns:       The address setup time, in nanoseconds. Usually
 *                             the maximum of tCLS, tCS and tALS. A negative
 *                             value indicates this characteristic isn't known.
 * @gpmi_sample_delay_in_ns:   A GPMI-specific timing parameter. A negative
 *                             value indicates this characteristic isn't known.
 * @tREA_in_ns:                tREA, in nanoseconds, from the data sheet. A
 *                             negative value indicates this characteristic
 *                             isn't known.
 * @tRLOH_in_ns:               tRLOH, in nanoseconds, from the data sheet. A
 *                             negative value indicates this characteristic
 *                             isn't known.
 * @tRHOH_in_ns:               tRHOH, in nanoseconds, from the data sheet. A
 *                             negative value indicates this characteristic
 *                             isn't known.
 */

struct nand_device_info {

	/* End of table marker */

	bool      end_of_table;

	/* Manufacturer and Device codes */

	uint8_t   manufacturer_code;
	uint8_t   device_code;

	/* Technology */

	enum nand_device_cell_technology  cell_technology;

	/* Geometry */

	uint64_t  chip_size_in_bytes;
	uint32_t  block_size_in_pages;
	uint16_t  page_total_size_in_bytes;

	/* ECC */

	uint8_t   ecc_strength_in_bits;
	uint16_t  ecc_size_in_bytes;

	/* Timing */

	int8_t    data_setup_in_ns;
	int8_t    data_hold_in_ns;
	int8_t    address_setup_in_ns;
	int8_t    gpmi_sample_delay_in_ns;
	int8_t    tREA_in_ns;
	int8_t    tRLOH_in_ns;
	int8_t    tRHOH_in_ns;

	/* Description */

	const char  *description;

};

/**
 * struct gpmi_nfc_info - i.MX NFC per-device data.
 *
 * Note that the "device" managed by this driver represents the NAND Flash
 * controller *and* the NAND Flash medium behind it. Thus, the per-device data
 * structure has information about the controller, the chips to which it is
 * connected, and properties of the medium as a whole.
 *
 * @dev:                 A pointer to the owning struct device.
 * @pdev:                A pointer to the owning struct platform_device.
 * @pdata:               A pointer to the device's platform data.
 * @resources:           Information about system resources used by this driver.
 * @device_info:         A structure that contains detailed information about
 *                       the NAND Flash device.
 * @physical_geometry:   A description of the medium's physical geometry.
 * @nfc:                 A pointer to a structure that represents the underlying
 *                       NFC hardware.
 * @nfc_geometry:        A description of the medium geometry as viewed by the
 *                       NFC.
 * @rom:                 A pointer to a structure that represents the underlying
 *                       Boot ROM.
 * @rom_geometry:        A description of the medium geometry as viewed by the
 *                       Boot ROM.
 * @mil:                 A collection of information used by the MTD Interface
 *                       Layer.
 */

struct gpmi_nfc_info {

	s32 cur_chip;
	u8  *data_buf;
	u8  *oob_buf;

	u8 m_u8MarkingBadBlock;
	u8 m_u8RawOOBMode;

	u32 m_u32EccChunkCnt;
	u32 m_u32EccStrength;
	u32 m_u32AuxSize;
	u32 m_u32AuxStsOfs;
	u32 m_u32BlkMarkByteOfs;
	u32 m_u32BlkMarkBitStart;

	int (*hooked_read_oob)(struct mtd_info *mtd,
				loff_t from, struct mtd_oob_ops *ops);
	int (*hooked_write_oob)(struct mtd_info *mtd,
				loff_t to, struct mtd_oob_ops *ops);
	int (*hooked_block_markbad)(struct mtd_info *mtd,
				loff_t ofs);

	/* NFC HAL */
	struct nfc_hal *nfc;
};


/**
 * struct gpmi_nfc_timing - GPMI NFC timing parameters
 *
 * This structure contains the fundamental timing attributes for the NAND Flash
 * bus and the GPMI NFC hardware.
 *
 * @data_setup_in_ns:         The data setup time, in nanoseconds. Usually the
 *                            maximum of tDS and tWP. A negative value
 *                            indicates this characteristic isn't known.
 * @data_hold_in_ns:          The data hold time, in nanoseconds. Usually the
 *                            maximum of tDH, tWH and tREH. A negative value
 *                            indicates this characteristic isn't known.
 * @address_setup_in_ns:      The address setup time, in nanoseconds. Usually
 *                            the maximum of tCLS, tCS and tALS. A negative
 *                            value indicates this characteristic isn't known.
 * @gpmi_sample_delay_in_ns:  A GPMI-specific timing parameter. A negative value
 *                            indicates this characteristic isn't known.
 * @tREA_in_ns:               tREA, in nanoseconds, from the data sheet. A
 *                            negative value indicates this characteristic isn't
 *                            known.
 * @tRLOH_in_ns:              tRLOH, in nanoseconds, from the data sheet. A
 *                            negative value indicates this characteristic isn't
 *                            known.
 * @tRHOH_in_ns:              tRHOH, in nanoseconds, from the data sheet. A
 *                            negative value indicates this characteristic isn't
 *                            known.
 */

struct gpmi_nfc_timing {
	u8 m_u8DataSetup;
	u8 m_u8DataHold;
	u8 m_u8AddressSetup;
	u8 m_u8HalfPeriods;
	u8 m_u8SampleDelay;
	u8 m_u8NandTimingState;
	u8 m_u8tREA;
	u8 m_u8tRLOH;
	u8 m_u8tRHOH;
};

/**
 * struct nfc_hal - GPMI NFC HAL
 *
 * This structure embodies an abstract interface to the underlying NFC hardware.
 *
 * @version:                     The NFC hardware version.
 * @description:                 A pointer to a human-readable description of
 *                               the NFC hardware.
 * @max_chip_count:              The maximum number of chips the NFC can
 *                               possibly support (this value is a constant for
 *                               each NFC version). This may *not* be the actual
 *                               number of chips connected.
 * @max_data_setup_cycles:       The maximum number of data setup cycles that
 *                               can be expressed in the hardware.
 * @internal_data_setup_in_ns:   The time, in ns, that the NFC hardware requires
 *                               for data read internal setup. In the Reference
 *                               Manual, see the chapter "High-Speed NAND
 *                               Timing" for more details.
 * @max_sample_delay_factor:     The maximum sample delay factor that can be
 *                               expressed in the hardware.
 * @max_dll_clock_period_in_ns:  The maximum period of the GPMI clock that the
 *                               sample delay DLL hardware can possibly work
 *                               with (the DLL is unusable with longer periods).
 *                               If the full-cycle period is greater than HALF
 *                               this value, the DLL must be configured to use
 *                               half-periods.
 * @max_dll_delay_in_ns:         The maximum amount of delay, in ns, that the
 *                               DLL can implement.
 * @dma_descriptors:             A pool of DMA descriptors.
 * @isr_dma_channel:             The DMA channel with which the NFC HAL is
 *                               working. We record this here so the ISR knows
 *                               which DMA channel to acknowledge.
 * @dma_done:                    The completion structure used for DMA
 *                               interrupts.
 * @bch_done:                    The completion structure used for BCH
 *                               interrupts.
 * @timing:                      The current timing configuration.
 * @clock_frequency_in_hz:       The clock frequency, in Hz, during the current
 *                               I/O transaction. If no I/O transaction is in
 *                               progress, this is the clock frequency during
 *                               the most recent I/O transaction.
 * @hardware_timing:             The hardware timing configuration in effect
 *                               during the current I/O transaction. If no I/O
 *                               transaction is in progress, this is the
 *                               hardware timing configuration during the most
 *                               recent I/O transaction.
 * @init:                        Initializes the NFC hardware and data
 *                               structures. This function will be called after
 *                               everything has been set up for communication
 *                               with the NFC itself, but before the platform
 *                               has set up off-chip communication. Thus, this
 *                               function must not attempt to communicate with
 *                               the NAND Flash hardware.
 * @set_geometry:                Configures the NFC hardware and data structures
 *                               to match the physical NAND Flash geometry.
 * @set_geometry:                Configures the NFC hardware and data structures
 *                               to match the physical NAND Flash geometry.
 * @set_timing:                  Configures the NFC hardware and data structures
 *                               to match the given NAND Flash bus timing.
 * @get_timing:                  Returns the the clock frequency, in Hz, and
 *                               the hardware timing configuration during the
 *                               current I/O transaction. If no I/O transaction
 *                               is in progress, this is the timing state during
 *                               the most recent I/O transaction.
 * @exit:                        Shuts down the NFC hardware and data
 *                               structures. This function will be called after
 *                               the platform has shut down off-chip
 *                               communication but while communication with the
 *                               NFC itself still works.
 * @clear_bch:                   Clears a BCH interrupt (intended to be called
 *                               by a more general interrupt handler to do
 *                               device-specific clearing).
 * @is_ready:                    Returns true if the given chip is ready.
 * @begin:                       Begins an interaction with the NFC. This
 *                               function must be called before *any* of the
 *                               following functions so the NFC can prepare
 *                               itself.
 * @end:                         Ends interaction with the NFC. This function
 *                               should be called to give the NFC a chance to,
 *                               among other things, enter a lower-power state.
 * @send_command:                Sends the given buffer of command bytes.
 * @send_data:                   Sends the given buffer of data bytes.
 * @read_data:                   Reads data bytes into the given buffer.
 * @send_page:                   Sends the given given data and OOB bytes,
 *                               using the ECC engine.
 * @read_page:                   Reads a page through the ECC engine and
 *                               delivers the data and OOB bytes to the given
 *                               buffers.
 */

#define  NFC_DMA_DESCRIPTOR_COUNT  (4)

struct nfc_hal {

	/* Hardware attributes. */

	const unsigned int      version;
	const char              *description;
	const unsigned int      max_chip_count;
	const unsigned int      max_data_setup_cycles;
	const unsigned int      internal_data_setup_in_ns;
	const unsigned int      max_sample_delay_factor;
	const unsigned int      max_dll_clock_period_in_ns;
	const unsigned int      max_dll_delay_in_ns;

	/* Working variables. */
	struct gpmi_nfc_timing  timing;
	unsigned long           clock_frequency_in_hz;

	/* Configuration functions. */

	int   (*init)        (void);
	int   (*set_geometry)(struct mtd_info *);
	int   (*set_timing)  (struct mtd_info *,
					const struct gpmi_nfc_timing *);
	void  (*get_timing)  (struct mtd_info *,
					unsigned long *clock_frequency_in_hz,
					struct gpmi_nfc_timing *);
	void  (*exit)        (struct mtd_info *);

	/* Call these functions to begin and end I/O. */

	void  (*begin)       (struct mtd_info *);
	void  (*end)         (struct mtd_info *);

	/* Call these I/O functions only between begin() and end(). */

	void  (*clear_bch)   (struct mtd_info *);
	int   (*is_ready)    (struct mtd_info *, unsigned chip);
	int   (*send_command)(struct mtd_info *, unsigned chip,
				dma_addr_t buffer, unsigned length);
	int   (*send_data)   (struct mtd_info *, unsigned chip,
				dma_addr_t buffer, unsigned length);
	int   (*read_data)   (struct mtd_info *, unsigned chip,
				dma_addr_t buffer, unsigned length);
	int   (*send_page)   (struct mtd_info *, unsigned chip,
				dma_addr_t payload, dma_addr_t auxiliary);
	int   (*read_page)   (struct mtd_info *, unsigned chip,
				dma_addr_t payload, dma_addr_t auxiliary);
};

/**
 * nand_device_get_info - Get info about a device based on ID bytes.
 *
 * @id_bytes:  An array of NAND_DEVICE_ID_BYTE_COUNT ID bytes retrieved from the
 *             NAND Flash device.
 */

struct nand_device_info *nand_device_get_info(const uint8_t id_bytes[]);

/**
 * nand_device_print_info - Prints information about a NAND Flash device.
 *
 * @info  A pointer to a NAND Flash device information structure.
 */

void nand_device_print_info(struct nand_device_info *info);

#endif
