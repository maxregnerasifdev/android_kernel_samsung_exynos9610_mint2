/*
 * arch/arm/mach-ep93xx/ts72xx.c
 * Technologic Systems TS72xx SBC support.
 *
 * Copyright (C) 2006 Lennert Buytenhek <buytenh@wantstofly.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/mtd/rawnand.h>
#include <linux/mtd/partitions.h>

#include <mach/hardware.h>

#include <asm/mach-types.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>

#include "soc.h"
#include "ts72xx.h"

static struct map_desc ts72xx_io_desc[] __initdata = {
	{
		.virtual	= (unsigned long)TS72XX_MODEL_VIRT_BASE,
		.pfn		= __phys_to_pfn(TS72XX_MODEL_PHYS_BASE),
		.length		= TS72XX_MODEL_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)TS72XX_OPTIONS_VIRT_BASE,
		.pfn		= __phys_to_pfn(TS72XX_OPTIONS_PHYS_BASE),
		.length		= TS72XX_OPTIONS_SIZE,
		.type		= MT_DEVICE,
	}, {
		.virtual	= (unsigned long)TS72XX_OPTIONS2_VIRT_BASE,
		.pfn		= __phys_to_pfn(TS72XX_OPTIONS2_PHYS_BASE),
		.length		= TS72XX_OPTIONS2_SIZE,
		.type		= MT_DEVICE,
	}
};

static void __init ts72xx_map_io(void)
{
	ep93xx_map_io();
	iotable_init(ts72xx_io_desc, ARRAY_SIZE(ts72xx_io_desc));
}


/*************************************************************************
 * NAND flash
 *************************************************************************/
#define TS72XX_NAND_CONTROL_ADDR_LINE	22	/* 0xN0400000 */
#define TS72XX_NAND_BUSY_ADDR_LINE	23	/* 0xN0800000 */

static void ts72xx_nand_hwcontrol(struct mtd_info *mtd,
				  int cmd, unsigned int ctrl)
{
	struct nand_chip *chip = mtd_to_nand(mtd);

	if (ctrl & NAND_CTRL_CHANGE) {
		void __iomem *addr = chip->IO_ADDR_R;
		unsigned char bits;

		addr += (1 << TS72XX_NAND_CONTROL_ADDR_LINE);

		bits = __raw_readb(addr) & ~0x07;
		bits |= (ctrl & NAND_NCE) << 2;	/* bit 0 -> bit 2 */
		bits |= (ctrl & NAND_CLE);	/* bit 1 -> bit 1 */
		bits |= (ctrl & NAND_ALE) >> 2;	/* bit 2 -> bit 0 */

		__raw_writeb(bits, addr);
	}

	if (cmd != NAND_CMD_NONE)
		__raw_writeb(cmd, chip->IO_ADDR_W);
}

static int ts72xx_nand_device_ready(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd_to_nand(mtd);
	void __iomem *addr = chip->IO_ADDR_R;

	addr += (1 << TS72XX_NAND_BUSY_ADDR_LINE);

	return !!(__raw_readb(addr) & 0x20);
}

#define TS72XX_BOOTROM_PART_SIZE	(SZ_16K)
#define TS72XX_REDBOOT_PART_SIZE	(SZ_2M + SZ_1M)

static struct mtd_partition ts72xx_nand_parts[] = {
	{
		.name		= "TS-BOOTROM",
		.offset		= 0,
		.size		= TS72XX_BOOTROM_PART_SIZE,
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	}, {
		.name		= "Linux",
		.offset		= MTDPART_OFS_RETAIN,
		.size		= TS72XX_REDBOOT_PART_SIZE,
				/* leave so much more for last partition */
	}, {
		.name		= "RedBoot",
		.offset		= MTDPART_OFS_APPEND,
		.size		= MTDPART_SIZ_FULL,
		.mask_flags	= MTD_WRITEABLE,	/* force read-only */
	},
};

static struct platform_nand_data ts72xx_nand_data = {
	.chip = {
		.nr_chips	= 1,
		.chip_offset	= 0,
		.chip_delay	= 15,
		.partitions	= ts72xx_nand_parts,
		.nr_partitions	= ARRAY_SIZE(ts72xx_nand_parts),
	},
	.ctrl = {
		.cmd_ctrl	= ts72xx_nand_hwcontrol,
		.dev_ready	= ts72xx_nand_device_ready,
	},
};

static struct resource ts72xx_nand_resource[] = {
	{
		.start		= 0,			/* filled in later */
		.end		= 0,			/* filled in later */
		.flags		= IORESOURCE_MEM,
	},
};

static struct platform_device ts72xx_nand_flash = {
	.name			= "gen_nand",
	.id			= -1,
	.dev.platform_data	= &ts72xx_nand_data,
	.resource		= ts72xx_nand_resource,
	.num_resources		= ARRAY_SIZE(ts72xx_nand_resource),
};


static void __init ts72xx_register_flash(void)
{
	/*
	 * TS7200 has NOR flash all other TS72xx board have NAND flash.
	 */
	if (board_is_ts7200()) {
		ep93xx_register_flash(2, EP93XX_CS6_PHYS_BASE, SZ_16M);
	} else {
		resource_size_t start;

		if (is_ts9420_installed())
			start = EP93XX_CS7_PHYS_BASE;
		else
			start = EP93XX_CS6_PHYS_BASE;

		ts72xx_nand_resource[0].start = start;
		ts72xx_nand_resource[0].end = start + SZ_16M - 1;

		platform_device_register(&ts72xx_nand_flash);
	}
}

/*************************************************************************
 * RTC M48T86
 *************************************************************************/
#define TS72XX_RTC_INDEX_PHYS_BASE	(EP93XX_CS1_PHYS_BASE + 0x00800000)
#define TS72XX_RTC_DATA_PHYS_BASE	(EP93XX_CS1_PHYS_BASE + 0x01700000)

static struct resource ts72xx_rtc_resources[] = {
	DEFINE_RES_MEM(TS72XX_RTC_INDEX_PHYS_BASE, 0x01),
	DEFINE_RES_MEM(TS72XX_RTC_DATA_PHYS_BASE, 0x01),
};

static struct platform_device ts72xx_rtc_device = {
	.name		= "rtc-m48t86",
	.id		= -1,
	.resource	= ts72xx_rtc_resources,
	.num_resources 	= ARRAY_SIZE(ts72xx_rtc_resources),
};

static struct resource ts72xx_wdt_resources[] = {
	{
		.start	= TS72XX_WDT_CONTROL_PHYS_BASE,
		.end	= TS72XX_WDT_CONTROL_PHYS_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= TS72XX_WDT_FEED_PHYS_BASE,
		.end	= TS72XX_WDT_FEED_PHYS_BASE + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ts72xx_wdt_device = {
	.name		= "ts72xx-wdt",
	.id		= -1,
	.num_resources 	= ARRAY_SIZE(ts72xx_wdt_resources),
	.resource	= ts72xx_wdt_resources,
};

static struct ep93xx_eth_data __initdata ts72xx_eth_data = {
	.phy_id		= 1,
};

#if IS_ENABLED(CONFIG_FPGA_MGR_TS73XX)

/* Relative to EP93XX_CS1_PHYS_BASE */
#define TS73XX_FPGA_LOADER_BASE		0x03c00000

static struct resource ts73xx_fpga_resources[] = {
	{
		.start	= EP93XX_CS1_PHYS_BASE + TS73XX_FPGA_LOADER_BASE,
		.end	= EP93XX_CS1_PHYS_BASE + TS73XX_FPGA_LOADER_BASE + 1,
		.flags	= IORESOURCE_MEM,
	},
};

static struct platform_device ts73xx_fpga_device = {
	.name	= "ts73xx-fpga-mgr",
	.id	= -1,
	.resource = ts73xx_fpga_resources,
	.num_resources = ARRAY_SIZE(ts73xx_fpga_resources),
};

#endif

static void __init ts72xx_init_machine(void)
{
	ep93xx_init_devices();
	ts72xx_register_flash();
	platform_device_register(&ts72xx_rtc_device);
	platform_device_register(&ts72xx_wdt_device);

	ep93xx_register_eth(&ts72xx_eth_data, 1);
#if IS_ENABLED(CONFIG_FPGA_MGR_TS73XX)
	if (board_is_ts7300())
		platform_device_register(&ts73xx_fpga_device);
#endif
}

MACHINE_START(TS72XX, "Technologic Systems TS-72xx SBC")
	/* Maintainer: Lennert Buytenhek <buytenh@wantstofly.org> */
	.atag_offset	= 0x100,
	.map_io		= ts72xx_map_io,
	.init_irq	= ep93xx_init_irq,
	.init_time	= ep93xx_timer_init,
	.init_machine	= ts72xx_init_machine,
	.init_late	= ep93xx_init_late,
	.restart	= ep93xx_restart,
MACHINE_END
