/*
 * (C) Copyright 2017 Whitebox Systems / Northend Systems B.V.
 * S.J.R. van Schaik <stephan@whiteboxsystems.nl>
 * M.B.W. Wajer <merlijn@whiteboxsystems.nl>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <errno.h>
#include <spi.h>

#include <asm/bitops.h>
#include <asm/gpio.h>
#include <asm/io.h>

#include <asm/arch/clock.h>
#include <asm/arch/spi.h>

/* local debug macro */
#undef SPI_DEBUG

#undef debug
#ifdef SPI_DEBUG
#define debug(fmt, args...)	printf(fmt, ##args)
#else
#define debug(fmt, args...)
#endif /* SPI_DEBUG */

#define SUNXI_SPI_MAX_RATE (24 * 1000 * 1000)
#define SUNXI_SPI_MIN_RATE (3 * 1000)

struct sunxi_spi_priv {
	struct sunxi_spi_regs *regs;
	unsigned int max_freq;
	const char *name;
	unsigned int activate_delay_us;
	unsigned int deactivate_delay_us;
	unsigned int last_transaction_us;
};

static void sunxi_spi_enable_clock(struct sunxi_spi_priv *priv)
{
	struct sunxi_ccm_reg * const ccm =
		(struct sunxi_ccm_reg * const)SUNXI_CCM_BASE;

#if defined(CONFIG_MACH_SUN6I) || defined(CONFIG_MACH_SUN8I) || \
	defined(CONFIG_MACH_SUN9I) || defined(CONFIG_MACH_SUN50I)
	setbits_le32(&ccm->ahb_reset0_cfg,
		(1 << AHB_GATE_OFFSET_SPI0));
#endif

	setbits_le32(&ccm->ahb_gate0, (1 << AHB_GATE_OFFSET_SPI0));
	writel((1 << 31), &ccm->spi0_clk_cfg);
}

static void sunxi_spi_disable_clock(void)
{
	struct sunxi_ccm_reg * const ccm =
		(struct sunxi_ccm_reg * const)SUNXI_CCM_BASE;

	writel(0, &ccm->spi0_clk_cfg);
	clrbits_le32(&ccm->ahb_gate0, (1 << AHB_GATE_OFFSET_SPI0));

#if defined(CONFIG_MACH_SUN6I) || defined(CONFIG_MACH_SUN8I) || \
	defined(CONFIG_MACH_SUN9I) || defined(CONFIG_MACH_SUN50I)
	clrbits_le32(&ccm->ahb_reset0_cfg,
		(1 << AHB_GATE_OFFSET_SPI0));
#endif
}

static void sunxi_spi_cs_activate(struct sunxi_spi_priv *priv, unsigned int cs)
{
	uint32_t reg;

	/* If it is too soon to perform another transaction, wait. */
	if (priv->deactivate_delay_us && priv->last_transaction_us) {
		unsigned int delay_us;

		delay_us = timer_get_us() - priv->last_transaction_us;

		if (delay_us < priv->deactivate_delay_us)
			udelay(priv->deactivate_delay_us - delay_us);
	}

	debug("%s: activate cs: %u, bus: '%s'\n", __func__, cs, priv->name);

	reg = readl(&priv->regs->xfer_ctl);
	reg &= ~(SUNXI_SPI_CTL_CS_MASK | SUNXI_SPI_CTL_CS_LEVEL);
	reg |= SUNXI_SPI_CTL_CS(cs);
	writel(reg, &priv->regs->xfer_ctl);

	if (priv->activate_delay_us)
		udelay(priv->activate_delay_us);
}

static void sunxi_spi_cs_deactivate(struct sunxi_spi_priv *priv, unsigned int cs)
{
	uint32_t reg;

	debug("%s: deactivate cs: %u, bus: '%s'\n", __func__, cs, priv->name);
	
	reg = readl(&priv->regs->xfer_ctl);
	reg &= ~SUNXI_SPI_CTL_CS_MASK;
	reg |= SUNXI_SPI_CTL_CS_LEVEL;
	writel(reg, &priv->regs->xfer_ctl);

	/* 
	 * Remember the time of this transaction so that we can honour the bus
	 * delay.
	 */
	if (priv->deactivate_delay_us)
		priv->last_transaction_us = timer_get_us();
}

int sunxi_spi_claim_bus(struct sunxi_spi_priv *priv)
{
	unsigned int pin_func = SUNXI_GPC_SPI0;

	debug("%s: claiming bus\n", __func__);

	sunxi_spi_enable_clock(priv);
	setbits_le32(&priv->regs->glb_ctl, SUNXI_SPI_CTL_MASTER |
		SUNXI_SPI_CTL_ENABLE | SUNXI_SPI_CTL_TP | SUNXI_SPI_CTL_SRST);

	if (IS_ENABLED(CONFIG_SUNXI_GEN_SUN6I))
		while (readl(&priv->regs->glb_ctl) & SUNXI_SPI_CTL_SRST)
			;

	setbits_le32(&priv->regs->xfer_ctl, SUNXI_SPI_CTL_CS_MANUAL |
		SUNXI_SPI_CTL_CS_LEVEL);
	setbits_le32(&priv->regs->fifo_ctl, SUNXI_SPI_CTL_RF_RST |
		SUNXI_SPI_CTL_TF_RST);

	return 0;
}

static int sunxi_spi_release_bus(struct sunxi_spi_priv *priv)
{
	debug("%s: releasing bus\n", __func__);

	clrbits_le32(&priv->regs->glb_ctl, SUNXI_SPI_CTL_MASTER |
		SUNXI_SPI_CTL_ENABLE);
	sunxi_spi_disable_clock();

	return 0;
}

static void sunxi_spi_write(struct sunxi_spi_priv *priv, const char *tx_buf,
	size_t nbytes)
{
	size_t i;
	char byte;

	if (!tx_buf)
		nbytes = 0;

	writel(SUNXI_SPI_XMIT_CNT(nbytes), &priv->regs->xmit_cnt);

	if (IS_ENABLED(CONFIG_SUNXI_GEN_SUN6I))
		writel(SUNXI_SPI_BURST_CNT(nbytes), &priv->regs->burst_ctl);

	for (i = 0; i < nbytes; ++i) {
		byte = tx_buf ? *tx_buf++ : 0;
		writeb(byte, &priv->regs->tx_data);
	}
}

int sunxi_spi_xfer(struct sunxi_spi_priv *priv, unsigned int bitlen, uint cs,
	const void *dout, void *din, unsigned long flags)
{
	const char *tx_buf = dout;
	char *rx_buf = din;
	size_t len = bitlen / 8;
	size_t i, nbytes;
	char byte;

	if (bitlen % 8) {
		debug("%s: non byte-aligned SPI transfer.\n", __func__);
		return -1;
	}

	if (flags & SPI_XFER_BEGIN)
		sunxi_spi_cs_activate(priv, cs);

	while (len) {
		nbytes = min(len, (size_t)64 - 1);

		writel(SUNXI_SPI_BURST_CNT(nbytes), &priv->regs->burst_cnt);
		sunxi_spi_write(priv, tx_buf, nbytes);
		setbits_le32(&priv->regs->xfer_ctl, SUNXI_SPI_CTL_XCH);

		while (((readl(&priv->regs->fifo_sta) &
			SUNXI_SPI_FIFO_RF_CNT_MASK) >>
			SUNXI_SPI_FIFO_RF_CNT_BITS) < nbytes)
			;

		for (i = 0; i < nbytes; ++i) {
			byte = readb(&priv->regs->rx_data);

			if (rx_buf)
				*rx_buf++ = byte;
		}

		len -= nbytes;

		if (tx_buf)
			tx_buf += nbytes;
	}

	if (flags & SPI_XFER_END)
		sunxi_spi_cs_deactivate(priv, cs);

	return 0;
}

int sunxi_spi_set_speed(struct sunxi_spi_priv *priv, uint speed)
{
	unsigned int div;
	uint32_t reg;

	speed = min(speed, (unsigned int)SUNXI_SPI_MAX_RATE);
	speed = max((unsigned int)SUNXI_SPI_MIN_RATE, speed);

	div = SUNXI_SPI_MAX_RATE / (2 * speed);

	if (div <= (SUNXI_SPI_CLK_CTL_CDR2_MASK + 1)) {
		if (div > 0)
			div--;

		reg = SUNXI_SPI_CLK_CTL_CDR2(div) | SUNXI_SPI_CLK_CTL_DRS;
	} else {
		div = __ilog2(SUNXI_SPI_MAX_RATE) - __ilog2(speed);
		reg = SUNXI_SPI_CLK_CTL_CDR1(div);
	}

	writel(reg, &priv->regs->clk_ctl);

	debug("%s: speed=%u\n", __func__, speed);

	return 0;
}

int sunxi_spi_set_mode(struct sunxi_spi_priv *priv, uint mode)
{
	uint32_t reg;

	reg = readl(&priv->regs->xfer_ctl);
	reg &= ~(SUNXI_SPI_CTL_CPOL | SUNXI_SPI_CTL_CPHA |
		SUNXI_SPI_CTL_CS_ACTIVE_LOW);

	if (mode & SPI_CPOL)
		reg |= SUNXI_SPI_CTL_CPOL;

	if (mode & SPI_CPHA)
		reg |= SUNXI_SPI_CTL_CPHA;

	if (!(mode & SPI_CS_HIGH))
		reg |= SUNXI_SPI_CTL_CS_ACTIVE_LOW;

	writel(reg, &priv->regs->xfer_ctl);

	debug("%s: mode=%d\n", __func__, mode);

	return 0;
}

static struct sunxi_spi_priv indev;
int sunxi_spi_probe(const char *name, ulong reg, void **dev)
{
	struct sunxi_spi_priv *priv = &indev;

    priv->name = name;
	priv->regs = (struct sunxi_spi_regs *)reg;
	priv->last_transaction_us = timer_get_us();
    *dev = priv;

	return 0;
}

