/*
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <tangliang@allwinnertech.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/timer.h>
#include <watchdog.h>

#define TIMER_MODE   (0x0 << 7)	/* continuous mode */
#define TIMER_DIV    (0x0 << 4)	/* pre scale 1 */
#define TIMER_SRC    (0x1 << 2)	/* osc24m */
#define TIMER_RELOAD (0x1 << 1)	/* reload internal value */
#define TIMER_EN     (0x1 << 0)	/* enable timer */

#define TIMER_CLOCK		(24 * 1000 * 1000)
#define COUNT_TO_USEC(x)	((x) / 24)
#define USEC_TO_COUNT(x)	((x) * 24)
#define TICKS_PER_HZ		(TIMER_CLOCK / CONFIG_SYS_HZ)
#define TICKS_TO_HZ(x)		((x) / TICKS_PER_HZ)

static ulong TIMER_LOAD_VAL = TIMER_CLOCK;
#define TIMER_NUM		0	/* we use timer 0 */

/* read the 32-bit timer */
ulong tick_read_timer(void)
{
	struct sunxi_timer_reg *timers = (struct sunxi_timer_reg *)SUNXI_TIMER_BASE;
	struct sunxi_timer *timer = &timers->timer[TIMER_NUM];

	/*
	 * The hardware timer counts down, therefore we invert to
	 * produce an incrementing timer.
	 */
	return TIMER_LOAD_VAL-readl(&timer->val);
}

/* init timer with tick */
int tick_timer_init(int tick)
{
	struct sunxi_timer_reg *timers = (struct sunxi_timer_reg *)SUNXI_TIMER_BASE;
	struct sunxi_timer *timer = &timers->timer[TIMER_NUM];
	TIMER_LOAD_VAL = TIMER_CLOCK/tick;
	writel(TIMER_LOAD_VAL, &timer->val);
	writel(TIMER_LOAD_VAL, &timer->inter);
	writel(TIMER_MODE | TIMER_DIV | TIMER_SRC | TIMER_RELOAD | TIMER_EN, &timer->ctl);
	writel(1<<TIMER_NUM, &timers->tirqen);

	return 0;
}

/* ack timer irq */
int tick_timer_ack(void)
{
	struct sunxi_timer_reg *timers = (struct sunxi_timer_reg *)SUNXI_TIMER_BASE;
	writel(1<<TIMER_NUM, &timers->tirqsta);

	return 0;
}

/* delay x useconds */
void __udelay(unsigned long usec)
{
	long tmo = USEC_TO_COUNT(usec);
	ulong now, last = tick_read_timer();

	while (tmo > 0) {
		now = tick_read_timer();
		if (now > last)	/* normal (non rollover) */
			tmo -= now - last;
		else		/* rollover */
			tmo -= TIMER_LOAD_VAL - last + now;
		last = now;
	}
}

#ifndef CONFIG_WD_PERIOD
# define CONFIG_WD_PERIOD	(10 * 1000 * 1000)	/* 10 seconds default */
#endif

extern ulong rt_tick_get();
ulong get_timer(ulong base)
{
	return rt_tick_get() - base;
}

unsigned long timer_get_us(void)
{
	return rt_tick_get() * 1000;
}

void udelay(unsigned long usec)
{
	ulong kv;

	do {
		WATCHDOG_RESET();
		kv = usec > CONFIG_WD_PERIOD ? CONFIG_WD_PERIOD : usec;
		__udelay (kv);
		usec -= kv;
	} while(usec);
}

void mdelay(unsigned long msec)
{
	while (msec--)
		udelay(1000);
}

