/*
 * Driver for Allwinner sun4i Pulse Width Modulation Controller
 *
 * Copyright (C) 2014 Alexandre Belloni <alexandre.belloni@free-electrons.com>
 *
 * Licensed under GPLv2.
 */

#include <common.h>
#include <errno.h>
#include <time.h>
#include <malloc.h>
#include <div64.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/cpu.h>
#include <asm/arch/gpio.h>
#include <asm/arch/mmc.h>
#include <asm-generic/gpio.h>

#define PWM_CTRL_REG		0x0

#define PWM_CH_PRD_BASE		0x4
#define PWM_CH_PRD_OFFSET	0x4
#define PWM_CH_PRD(ch)		(PWM_CH_PRD_BASE + PWM_CH_PRD_OFFSET * (ch))

#define PWMCH_OFFSET		15
#define PWM_PRESCAL_MASK	GENMASK(3, 0)
#define PWM_PRESCAL_OFF		0
#define PWM_EN			BIT(4)
#define PWM_ACT_STATE		BIT(5)
#define PWM_CLK_GATING		BIT(6)
#define PWM_MODE		BIT(7)
#define PWM_PULSE		BIT(8)
#define PWM_BYPASS		BIT(9)

#define PWM_RDY_BASE		28
#define PWM_RDY_OFFSET		1
#define PWM_RDY(ch)		BIT(PWM_RDY_BASE + PWM_RDY_OFFSET * (ch))

#define PWM_PRD(prd)		(((prd) - 1) << 16)
#define PWM_PRD_MASK		GENMASK(15, 0)

#define PWM_DTY_MASK		GENMASK(15, 0)

#define BIT_CH(bit, chan)	((bit) << ((chan) * PWMCH_OFFSET))

#define NSEC_PER_SEC			1000000000L
#define PWM_POLARITY_NORMAL		0
#define PWM_POLARITY_INVERSED	1

#define clk_prepare_enable(x)	0
#define clk_disable_unprepare(x)
#define clk_get_rate(x) (x)

static const u32 prescaler_table[] = {
	120,
	180,
	240,
	360,
	480,
	0,
	0,
	0,
	12000,
	24000,
	36000,
	48000,
	72000,
	0,
	0,
	0, /* Actually 1 but tested separately */
};

struct sun4i_pwm_data {
	bool has_prescaler_bypass;
	bool has_rdy;
	unsigned int npwm;
};

struct sun4i_pwm_chip {
	u32 base;
	u32 clk;
	const struct sun4i_pwm_data *data;
};

static struct sun4i_pwm_chip indev[2];
static inline u32 sun4i_pwm_readl(struct sun4i_pwm_chip *chip,
				  unsigned long offset)
{
	return readl(chip->base + offset);
}

static inline void sun4i_pwm_writel(struct sun4i_pwm_chip *chip,
				    u32 val, unsigned long offset)
{
	writel(val, chip->base + offset);
}

int sun4i_pwm_config(int hwpwm, int duty_ns, int period_ns)
{
	struct sun4i_pwm_chip *sun4i_pwm = &indev[hwpwm];
	u32 prd, dty, val, clk_gate;
	u64 clk_rate, div = 0;
	unsigned int prescaler = 0;
	int err;

	clk_rate = clk_get_rate(sun4i_pwm->clk);

	if (sun4i_pwm->data->has_prescaler_bypass) {
		/* First, test without any prescaler when available */
		prescaler = PWM_PRESCAL_MASK;
		/*
		 * When not using any prescaler, the clock period in nanoseconds
		 * is not an integer so round it half up instead of
		 * truncating to get less surprising values.
		 */
		div = clk_rate * period_ns + NSEC_PER_SEC / 2;
		do_div(div, NSEC_PER_SEC);
		if (div - 1 > PWM_PRD_MASK)
			prescaler = 0;
	}

	if (prescaler == 0) {
		/* Go up from the first divider */
		for (prescaler = 0; prescaler < PWM_PRESCAL_MASK; prescaler++) {
			if (!prescaler_table[prescaler])
				continue;
			div = clk_rate;
			do_div(div, prescaler_table[prescaler]);
			div = div * period_ns;
			do_div(div, NSEC_PER_SEC);
			if (div - 1 <= PWM_PRD_MASK)
				break;
		}

		if (div - 1 > PWM_PRD_MASK) {
			debug("period exceeds the maximum value\n");
			return -EINVAL;
		}
	}

	prd = div;
	div *= duty_ns;
	do_div(div, period_ns);
	dty = div;

	err = clk_prepare_enable(sun4i_pwm->clk);
	if (err) {
		debug("failed to enable PWM clock\n");
		return err;
	}

	val = sun4i_pwm_readl(sun4i_pwm, PWM_CTRL_REG);

	if (sun4i_pwm->data->has_rdy && (val & PWM_RDY(hwpwm))) {
		clk_disable_unprepare(sun4i_pwm->clk);
		return -EBUSY;
	}

	clk_gate = val & BIT_CH(PWM_CLK_GATING, hwpwm);
	if (clk_gate) {
		val &= ~BIT_CH(PWM_CLK_GATING, hwpwm);
		sun4i_pwm_writel(sun4i_pwm, val, PWM_CTRL_REG);
	}

	val = sun4i_pwm_readl(sun4i_pwm, PWM_CTRL_REG);
	val &= ~BIT_CH(PWM_PRESCAL_MASK, hwpwm);
	val |= BIT_CH(prescaler, hwpwm);
	sun4i_pwm_writel(sun4i_pwm, val, PWM_CTRL_REG);

	val = (dty & PWM_DTY_MASK) | PWM_PRD(prd);
	sun4i_pwm_writel(sun4i_pwm, val, PWM_CH_PRD(hwpwm));

	if (clk_gate) {
		val = sun4i_pwm_readl(sun4i_pwm, PWM_CTRL_REG);
		val |= clk_gate;
		sun4i_pwm_writel(sun4i_pwm, val, PWM_CTRL_REG);
	}

	clk_disable_unprepare(sun4i_pwm->clk);

	return 0;
}

int sun4i_pwm_set_polarity(int hwpwm, int polarity)
{
	struct sun4i_pwm_chip *sun4i_pwm = &indev[hwpwm];
	u32 val;
	int ret;

	ret = clk_prepare_enable(sun4i_pwm->clk);
	if (ret) {
		debug("failed to enable PWM clock\n");
		return ret;
	}

	val = sun4i_pwm_readl(sun4i_pwm, PWM_CTRL_REG);

	if (polarity != PWM_POLARITY_NORMAL)
		val &= ~BIT_CH(PWM_ACT_STATE, hwpwm);
	else
		val |= BIT_CH(PWM_ACT_STATE, hwpwm);

	sun4i_pwm_writel(sun4i_pwm, val, PWM_CTRL_REG);

	clk_disable_unprepare(sun4i_pwm->clk);

	return 0;
}

int sun4i_pwm_enable(int hwpwm)
{
	struct sun4i_pwm_chip *sun4i_pwm = &indev[hwpwm];
	u32 val;
	int ret;

	ret = clk_prepare_enable(sun4i_pwm->clk);
	if (ret) {
		debug("failed to enable PWM clock\n");
		return ret;
	}

	val = sun4i_pwm_readl(sun4i_pwm, PWM_CTRL_REG);
	val |= BIT_CH(PWM_EN, hwpwm);
	val |= BIT_CH(PWM_CLK_GATING, hwpwm);
	sun4i_pwm_writel(sun4i_pwm, val, PWM_CTRL_REG);

	return 0;
}

void sun4i_pwm_disable(int hwpwm)
{
	struct sun4i_pwm_chip *sun4i_pwm = &indev[hwpwm];
	u32 val;

	val = sun4i_pwm_readl(sun4i_pwm, PWM_CTRL_REG);
	val &= ~BIT_CH(PWM_EN, hwpwm);
	val &= ~BIT_CH(PWM_CLK_GATING, hwpwm);
	sun4i_pwm_writel(sun4i_pwm, val, PWM_CTRL_REG);

	clk_disable_unprepare(sun4i_pwm->clk);
}

static const struct sun4i_pwm_data sun4i_pwm_data_a20 = {
	.has_prescaler_bypass = true,
	.has_rdy = true,
	.npwm = 2,
};

int sun4i_pwm_probe(u32 base, u32 clk)
{
	struct sun4i_pwm_chip *pwm = &indev[0];
	pwm->base = base;
	pwm->clk = clk;
	pwm->data = &sun4i_pwm_data_a20;
	pwm = &indev[1];
	pwm->base = base;
	pwm->clk = clk;
	pwm->data = &sun4i_pwm_data_a20;

	return 0;
}
