/*
 * (C) Copyright 2017 Whitebox Systems / Northend Systems B.V.
 * S.J.R. van Schaik <stephan@whiteboxsystems.nl>
 * M.B.W. Wajer <merlijn@whiteboxsystems.nl>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef _SUNXI_SPI_SUN6I_H
#define _SUNXI_SPI_SUN6I_H

struct sunxi_spi_regs {
	u32 unused0[1];
	u32 glb_ctl;	/* 0x04 */
	u32 xfer_ctl;	/* 0x08 */
	u32 unused1[1];
	u32 int_ctl;	/* 0x10 */
	u32 int_sta;	/* 0x14 */
	u32 fifo_ctl;	/* 0x18 */
	u32 fifo_sta;	/* 0x1c */
	u32 wait;		/* 0x20 */
	u32 clk_ctl;	/* 0x24 */
	u32 unused2[2];
	u32 burst_cnt;	/* 0x30 */
	u32 xmit_cnt;	/* 0x34 */
	u32 burst_ctl;	/* 0x38 */
	u32 unused3[113];
	u32 tx_data;	/* 0x200 */
	u32 unused4[63];
	u32 rx_data;	/* 0x300 */
};

#define SUNXI_SPI_CTL_ENABLE		BIT(0)
#define SUNXI_SPI_CTL_MASTER		BIT(1)
#define SUNXI_SPI_CTL_TP		BIT(7)
#define SUNXI_SPI_CTL_SRST		BIT(31)

#define SUNXI_SPI_CTL_CPHA		BIT(0)
#define SUNXI_SPI_CTL_CPOL		BIT(1)
#define SUNXI_SPI_CTL_CS_ACTIVE_LOW	BIT(2)
#define SUNXI_SPI_CTL_CS_MASK		0x30
#define SUNXI_SPI_CTL_CS(cs)		(((cs) << 4) & SUNXI_SPI_CTL_CS_MASK)
#define SUNXI_SPI_CTL_CS_MANUAL		BIT(6)
#define SUNXI_SPI_CTL_CS_LEVEL		BIT(7)
#define SUNXI_SPI_CTL_DHB		BIT(8)
#define SUNXI_SPI_CTL_XCH		BIT(31)

#define SUNXI_SPI_CTL_RF_RST		BIT(15)
#define SUNXI_SPI_CTL_TF_RST		BIT(31)

#define SUNXI_SPI_FIFO_RF_CNT_MASK	0x7f
#define SUNXI_SPI_FIFO_RF_CNT_BITS	0
#define SUNXI_SPI_FIFO_TF_CNT_MASK	0x7f
#define SUNXI_SPI_FIFO_TF_CNT_BITS	16

#endif /* _SUNXI_SPI_SUN6I_H */
