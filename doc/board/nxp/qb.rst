.. SPDX-License-Identifier: GPL-2.0+
   Copyright 2026 NXP

DDR QuickBoot flow
------------------

Some NXP boards (which use OEI - iMX943, iMX95, etc.) support saving DDR
training data (collected by OEI during Training flow) from volatile
to non-volatile memory, which is then available to OEI at next cold reboot.
OEI uses the saved data to run Quickboot flow and avoid training the DDR again.
This significantly reduces the boot time.

U-Boot provides no authentication for qb data, only its integrity
is verified via the CRC32. The authentication is done in OEI. With
the exception of iMX95 A0/A1, which use CRC32 as well for verifying
the data, the rest of the boards use ELE to verify the MAC stored
in the ddrphy_qb_state structure.

If the quickboot data in memory is not valid (CRC32 check fails),
U-Boot does not save it to NVM. So, if OEI runs Quickboot flow -> no
data is written to volatile memory -> invalid data -> no saving happens
(qb save fails during qb check).

After successful saving, U-Boot clears the data in volatile memory so
that qb check fails at next reboot and the NVM isn't accessed again.

There are 2 ways to save this data (both can be enabled):

1. automatically, in SPL (by enabling CONFIG_SPL_IMX_QB)

- this will save the data on the current boot device (e.g. SD)
- other configs specific to the boot device need to be enabled (CONFIG_SPL_MMC_WRITE for saving to eMMC/SD)
- use for: automating qb save / saving qb data if using Falcon mode (skipping U-Boot proper)

2. using qb command in U-Boot console (by enabling CONFIG_CMD_IMX_QB)

- supports saving on the current boot device, or on another, specified device.
- if flashing via uuu, the command can be added in an uuu script (boot device needs to be specified)
- use for: saving the qb data during flashing / controlling the NVM to save to

::

        # To save/erase on current boot device
        => qb save/erase

        # To save/erase on other boot device
        => qb save/erase mmc 0 # eMMC
        => qb save/erase mmc 1 # SD
        => qb save/erase spi   # NOR SPI
