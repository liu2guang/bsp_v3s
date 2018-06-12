/*
 * An RTC driver for Allwinner A31/A23
 *
 * Copyright (c) 2014, Chen-Yu Tsai <wens@csie.org>
 *
 * based on rtc-sunxi.c
 *
 * An RTC driver for Allwinner A10/A20
 *
 * Copyright (c) 2013, Carlo Caione <carlo.caione@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
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

/* Control register */
#define SUN6I_LOSC_CTRL				0x0000
#define SUN6I_LOSC_CTRL_KEY			(0x16aa << 16)
#define SUN6I_LOSC_CTRL_ALM_DHMS_ACC		BIT(9)
#define SUN6I_LOSC_CTRL_RTC_HMS_ACC		BIT(8)
#define SUN6I_LOSC_CTRL_RTC_YMD_ACC		BIT(7)
#define SUN6I_LOSC_CTRL_EXT_OSC			BIT(0)
#define SUN6I_LOSC_CTRL_ACC_MASK		GENMASK(9, 7)

/* RTC */
#define SUN6I_RTC_YMD				0x0010
#define SUN6I_RTC_HMS				0x0014

/* Alarm 0 (counter) */
#define SUN6I_ALRM_COUNTER			0x0020
#define SUN6I_ALRM_CUR_VAL			0x0024
#define SUN6I_ALRM_EN				0x0028
#define SUN6I_ALRM_EN_CNT_EN			BIT(0)
#define SUN6I_ALRM_IRQ_EN			0x002c
#define SUN6I_ALRM_IRQ_EN_CNT_IRQ_EN		BIT(0)
#define SUN6I_ALRM_IRQ_STA			0x0030
#define SUN6I_ALRM_IRQ_STA_CNT_IRQ_PEND		BIT(0)

/* Alarm 1 (wall clock) */
#define SUN6I_ALRM1_EN				0x0044
#define SUN6I_ALRM1_IRQ_EN			0x0048
#define SUN6I_ALRM1_IRQ_STA			0x004c
#define SUN6I_ALRM1_IRQ_STA_WEEK_IRQ_PEND	BIT(0)

/* Alarm config */
#define SUN6I_ALARM_CONFIG			0x0050
#define SUN6I_ALARM_CONFIG_WAKEUP		BIT(0)

/*
 * Get date values
 */
#define SUN6I_DATE_GET_DAY_VALUE(x)		((x)  & 0x0000001f)
#define SUN6I_DATE_GET_MON_VALUE(x)		(((x) & 0x00000f00) >> 8)
#define SUN6I_DATE_GET_YEAR_VALUE(x)		(((x) & 0x003f0000) >> 16)
#define SUN6I_LEAP_GET_VALUE(x)			(((x) & 0x00400000) >> 22)

/*
 * Get time values
 */
#define SUN6I_TIME_GET_SEC_VALUE(x)		((x)  & 0x0000003f)
#define SUN6I_TIME_GET_MIN_VALUE(x)		(((x) & 0x00003f00) >> 8)
#define SUN6I_TIME_GET_HOUR_VALUE(x)		(((x) & 0x001f0000) >> 16)

/*
 * Set date values
 */
#define SUN6I_DATE_SET_DAY_VALUE(x)		((x)       & 0x0000001f)
#define SUN6I_DATE_SET_MON_VALUE(x)		((x) <<  8 & 0x00000f00)
#define SUN6I_DATE_SET_YEAR_VALUE(x)		((x) << 16 & 0x003f0000)
#define SUN6I_LEAP_SET_VALUE(x)			((x) << 22 & 0x00400000)

/*
 * Set time values
 */
#define SUN6I_TIME_SET_SEC_VALUE(x)		((x)       & 0x0000003f)
#define SUN6I_TIME_SET_MIN_VALUE(x)		((x) <<  8 & 0x00003f00)
#define SUN6I_TIME_SET_HOUR_VALUE(x)		((x) << 16 & 0x001f0000)

/*
 * The year parameter passed to the driver is usually an offset relative to
 * the year 1900. This macro is used to convert this offset to another one
 * relative to the minimum year allowed by the hardware.
 *
 * The year range is 1970 - 2033. This range is selected to match Allwinner's
 * driver, even though it is somewhat limited.
 */
#define SUN6I_YEAR_MIN				1970
#define SUN6I_YEAR_MAX				2033
#define SUN6I_YEAR_OFF				(SUN6I_YEAR_MIN - 1900)

static const unsigned char rtc_days_in_month[] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static const unsigned short rtc_ydays[2][13] = {
	/* Normal years */
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
	/* Leap years */
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

#define LEAPS_THRU_END_OF(y) ((y)/4 - (y)/100 + (y)/400)
static inline bool is_leap_year(unsigned int year){return (!(year % 4) && (year % 100)) || !(year % 400);}

/*
 * The number of days in the month.
 */
static int rtc_month_days(unsigned int month, unsigned int year)
{
	return rtc_days_in_month[month] + (is_leap_year(year) && month == 1);
}

/*
 * Does the rtc_time represent a valid date/time?
 */
static int rtc_valid_tm(struct tm *rtc_tm)
{
	if (rtc_tm->tm_year < 70
		|| ((unsigned)rtc_tm->tm_mon) >= 12
		|| rtc_tm->tm_mday < 1
		|| rtc_tm->tm_mday > rtc_month_days(rtc_tm->tm_mon, rtc_tm->tm_year + 1900)
		|| ((unsigned)rtc_tm->tm_hour) >= 24
		|| ((unsigned)rtc_tm->tm_min) >= 60
		|| ((unsigned)rtc_tm->tm_sec) >= 60)
		return -EINVAL;

	return 0;
}

int sun6i_rtc_gettime(u32 base, struct tm *rtc_tm)
{
	u32 date, time;

	/*
	 * read again in case it changes
	 */
	do {
		date = readl(base + SUN6I_RTC_YMD);
		time = readl(base + SUN6I_RTC_HMS);
	} while ((date != readl(base + SUN6I_RTC_YMD)) ||
		 (time != readl(base + SUN6I_RTC_HMS)));

	rtc_tm->tm_sec  = SUN6I_TIME_GET_SEC_VALUE(time);
	rtc_tm->tm_min  = SUN6I_TIME_GET_MIN_VALUE(time);
	rtc_tm->tm_hour = SUN6I_TIME_GET_HOUR_VALUE(time);

	rtc_tm->tm_mday = SUN6I_DATE_GET_DAY_VALUE(date);
	rtc_tm->tm_mon  = SUN6I_DATE_GET_MON_VALUE(date);
	rtc_tm->tm_year = SUN6I_DATE_GET_YEAR_VALUE(date);

	rtc_tm->tm_mon  -= 1;

	/*
	 * switch from (data_year->min)-relative offset to
	 * a (1900)-relative one
	 */
	rtc_tm->tm_year += SUN6I_YEAR_OFF;

	return rtc_valid_tm(rtc_tm);
}

static int sun6i_rtc_wait(u32 base, int offset, unsigned int mask, unsigned int ms_timeout)
{
	ulong start = get_timer(0);
	u32 reg;

	do {
		reg = readl(base + offset);
		reg &= mask;

		if (!reg)
			return 0;

		udelay(10);
	} while (get_timer(start) < ms_timeout);

	return -ETIMEDOUT;
}

int sun6i_rtc_settime(u32 base, struct tm *rtc_tm)
{
	u32 date = 0;
	u32 time = 0;
	int year;

	year = rtc_tm->tm_year + 1900;
	if (year < SUN6I_YEAR_MIN || year > SUN6I_YEAR_MAX) {
		debug("rtc only supports year in range %d - %d\n",
			SUN6I_YEAR_MIN, SUN6I_YEAR_MAX);
		return -EINVAL;
	}

	rtc_tm->tm_year -= SUN6I_YEAR_OFF;
	rtc_tm->tm_mon += 1;

	date = SUN6I_DATE_SET_DAY_VALUE(rtc_tm->tm_mday) |
		SUN6I_DATE_SET_MON_VALUE(rtc_tm->tm_mon)  |
		SUN6I_DATE_SET_YEAR_VALUE(rtc_tm->tm_year);

	if (is_leap_year(year))
		date |= SUN6I_LEAP_SET_VALUE(1);

	time = SUN6I_TIME_SET_SEC_VALUE(rtc_tm->tm_sec)  |
		SUN6I_TIME_SET_MIN_VALUE(rtc_tm->tm_min)  |
		SUN6I_TIME_SET_HOUR_VALUE(rtc_tm->tm_hour);

	/* Check whether registers are writable */
	if (sun6i_rtc_wait(base, SUN6I_LOSC_CTRL, SUN6I_LOSC_CTRL_ACC_MASK, 50)) {
		debug("rtc is still busy.\n");
		return -EBUSY;
	}

	writel(time, base + SUN6I_RTC_HMS);

	/*
	 * After writing the RTC HH-MM-SS register, the
	 * SUN6I_LOSC_CTRL_RTC_HMS_ACC bit is set and it will not
	 * be cleared until the real writing operation is finished
	 */

	if (sun6i_rtc_wait(base, SUN6I_LOSC_CTRL, SUN6I_LOSC_CTRL_RTC_HMS_ACC, 50)) {
		debug("Failed to set rtc time.\n");
		return -ETIMEDOUT;
	}

	writel(date, base + SUN6I_RTC_YMD);

	/*
	 * After writing the RTC YY-MM-DD register, the
	 * SUN6I_LOSC_CTRL_RTC_YMD_ACC bit is set and it will not
	 * be cleared until the real writing operation is finished
	 */

	if (sun6i_rtc_wait(base, SUN6I_LOSC_CTRL, SUN6I_LOSC_CTRL_RTC_YMD_ACC, 50)) {
		debug("Failed to set rtc time.\n");
		return -ETIMEDOUT;
	}

	return 0;
}

int sun6i_rtc_probe(u32 base)
{
	/* clear the alarm counter value */
	writel(0, base + SUN6I_ALRM_COUNTER);

	/* disable counter alarm */
	writel(0, base + SUN6I_ALRM_EN);

	/* disable counter alarm interrupt */
	writel(0, base + SUN6I_ALRM_IRQ_EN);

	/* disable week alarm */
	writel(0, base + SUN6I_ALRM1_EN);

	/* disable week alarm interrupt */
	writel(0, base + SUN6I_ALRM1_IRQ_EN);

	/* clear counter alarm pending interrupts */
	writel(SUN6I_ALRM_IRQ_STA_CNT_IRQ_PEND, base + SUN6I_ALRM_IRQ_STA);

	/* clear week alarm pending interrupts */
	writel(SUN6I_ALRM1_IRQ_STA_WEEK_IRQ_PEND, base + SUN6I_ALRM1_IRQ_STA);

	/* disable alarm wakeup */
	writel(0, base + SUN6I_ALARM_CONFIG);

	/* switch to the external, more precise, oscillator */
	writel(SUN6I_LOSC_CTRL_KEY | SUN6I_LOSC_CTRL_EXT_OSC, base + SUN6I_LOSC_CTRL);

	return 0;
}
