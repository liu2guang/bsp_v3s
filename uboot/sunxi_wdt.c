/*
 *      sunxi Watchdog Driver
 *
 *      Copyright (c) 2013 Carlo Caione
 *                    2012 Henrik Nordstrom
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 *
 *      Based on xen_wdt.c
 *      (c) Copyright 2010 Novell, Inc.
 */

#include <common.h>
#include <errno.h>
#include <time.h>
#include <malloc.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gpio.h>
#include <asm/arch/mmc.h>
#include <asm-generic/gpio.h>

#define WDT_MAX_TIMEOUT         16
#define WDT_MIN_TIMEOUT         1
#define WDT_TIMEOUT_MASK        0x0F

#define WDT_CTRL_RELOAD         ((1 << 0) | (0x0a57 << 1))

#define WDT_MODE_EN             (1 << 0)

/*
 * This structure stores the register offsets for different variants
 * of Allwinner's watchdog hardware.
 */
struct sunxi_wdt_reg {
	u8 wdt_ctrl;
	u8 wdt_cfg;
	u8 wdt_mode;
	u8 wdt_timeout_shift;
	u8 wdt_reset_mask;
	u8 wdt_reset_val;
};

struct sunxi_wdt_dev {
	u32 wdt_base;
	const struct sunxi_wdt_reg *wdt_regs;
};

/*
 * wdt_timeout_map maps the watchdog timer interval value in seconds to
 * the value of the register WDT_MODE at bits .wdt_timeout_shift ~ +3
 *
 * [timeout seconds] = register value
 *
 */

static const int wdt_timeout_map[] = {
	[1] = 0x1,  /* 1s  */
	[2] = 0x2,  /* 2s  */
	[3] = 0x3,  /* 3s  */
	[4] = 0x4,  /* 4s  */
	[5] = 0x5,  /* 5s  */
	[6] = 0x6,  /* 6s  */
	[8] = 0x7,  /* 8s  */
	[10] = 0x8, /* 10s */
	[12] = 0x9, /* 12s */
	[14] = 0xA, /* 14s */
	[16] = 0xB, /* 16s */
};

int sunxi_wdt_ping(struct sunxi_wdt_dev *sunxi_wdt)
{
	u32 wdt_base = sunxi_wdt->wdt_base;
	const struct sunxi_wdt_reg *regs = sunxi_wdt->wdt_regs;

	writel(WDT_CTRL_RELOAD, wdt_base + regs->wdt_ctrl);

	return 0;
}

static int sunxi_wdt_set_timeout(struct sunxi_wdt_dev *sunxi_wdt, unsigned int timeout)
{
	u32 wdt_base = sunxi_wdt->wdt_base;
	const struct sunxi_wdt_reg *regs = sunxi_wdt->wdt_regs;
	u32 reg;

	if (wdt_timeout_map[timeout] == 0)
		timeout++;

	reg = readl(wdt_base + regs->wdt_mode);
	reg &= ~(WDT_TIMEOUT_MASK << regs->wdt_timeout_shift);
	reg |= wdt_timeout_map[timeout] << regs->wdt_timeout_shift;
	writel(reg, wdt_base + regs->wdt_mode);

	sunxi_wdt_ping(sunxi_wdt);

	return 0;
}

int sunxi_wdt_stop(struct sunxi_wdt_dev *sunxi_wdt)
{
	u32 wdt_base = sunxi_wdt->wdt_base;
	const struct sunxi_wdt_reg *regs = sunxi_wdt->wdt_regs;

	writel(0, wdt_base + regs->wdt_mode);

	return 0;
}

int sunxi_wdt_start(struct sunxi_wdt_dev *sunxi_wdt, int timeout)
{
	u32 reg;
	u32 wdt_base = sunxi_wdt->wdt_base;
	const struct sunxi_wdt_reg *regs = sunxi_wdt->wdt_regs;
	int ret;

	ret = sunxi_wdt_set_timeout(sunxi_wdt, timeout);
	if (ret < 0)
		return ret;

	/* Set system reset function */
	reg = readl(wdt_base + regs->wdt_cfg);
	reg &= ~(regs->wdt_reset_mask);
	reg |= regs->wdt_reset_val;
	writel(reg, wdt_base + regs->wdt_cfg);

	/* Enable watchdog */
	reg = readl(wdt_base + regs->wdt_mode);
	reg |= WDT_MODE_EN;
	writel(reg, wdt_base + regs->wdt_mode);

	return 0;
}

static const struct sunxi_wdt_reg sun6i_wdt_reg = {
	.wdt_ctrl = 0x10,
	.wdt_cfg = 0x14,
	.wdt_mode = 0x18,
	.wdt_timeout_shift = 4,
	.wdt_reset_mask = 0x03,
	.wdt_reset_val = 0x01,
};

static struct sunxi_wdt_dev indev;
struct sunxi_wdt_dev* sunxi_wdt_probe(u32 base)
{
	struct sunxi_wdt_dev *sunxi_wdt = &indev;

	sunxi_wdt->wdt_regs = &sun6i_wdt_reg;
	sunxi_wdt->wdt_base = base;
	sunxi_wdt_stop(sunxi_wdt);

	return sunxi_wdt;
}
