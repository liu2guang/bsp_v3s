/*
 * File      : drv_uart.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2017, RT-Thread Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Change Logs:
 * Date           Author       Notes
 * 2017-5-30      Bernard      the first version
 */

#include <rthw.h>
#include <rtthread.h>
#include <rtdevice.h>

#include "board.h"
#include "interrupt.h"
#include <stdlib.h>
#include <time.h>

#define SUNXI_RTC_BASE 0x1c20400
extern int sun6i_rtc_gettime(uint32_t base, struct tm *rtc_tm);
static int _rtc_gettime(time_t *time)
{
    struct tm _tm;
    int ret = sun6i_rtc_gettime(SUNXI_RTC_BASE, &_tm);
    if (ret != 0) *time = 0; else *time = mktime(&_tm);
 	return ret;
}

extern int sun6i_rtc_settime(uint32_t base, struct tm *rtc_tm);
static int _rtc_settime(time_t *time)
{
    struct tm _tm = *localtime(time);
	return sun6i_rtc_settime(SUNXI_RTC_BASE, &_tm);
}

static struct rt_mutex rtc_mutex;
static rt_err_t rt_rtc_open(rt_device_t dev, rt_uint16_t oflag)
{
    return RT_EOK;
}

static rt_size_t rt_rtc_read(rt_device_t dev, rt_off_t pos, void* buffer, rt_size_t size)
{
    return RT_EOK;
}

static rt_err_t rt_rtc_init(rt_device_t dev)
{
    return RT_EOK;
}

static rt_err_t rt_rtc_control(rt_device_t dev, int cmd, void *args)
{
    time_t *time = (time_t *)args;

    RT_ASSERT(dev != RT_NULL);
    rt_mutex_take(&rtc_mutex, RT_WAITING_FOREVER);
	
    switch (cmd)
    {
    case RT_DEVICE_CTRL_RTC_GET_TIME:
        _rtc_gettime(time);
        break;

    case RT_DEVICE_CTRL_RTC_SET_TIME:
        _rtc_settime(time);
        break;
    }

	rt_mutex_release(&rtc_mutex);
    return RT_EOK;
}

static struct rt_device rtc;
extern int sun6i_rtc_probe(uint32_t base);
int rt_hw_rtc_init(void)
{
    sun6i_rtc_probe(SUNXI_RTC_BASE);

    /* register rtc device */
    rtc.type	= RT_Device_Class_RTC;
    rtc.init 	= rt_rtc_init;
    rtc.open 	= rt_rtc_open;
    rtc.close	= RT_NULL;
    rtc.read 	= rt_rtc_read;
    rtc.write	= RT_NULL;
    rtc.control = rt_rtc_control;

    /* no private */
    rtc.user_data = RT_NULL;

	rt_mutex_init(&rtc_mutex, "rtc", RT_IPC_FLAG_FIFO);
    rt_device_register(&rtc, "rtc", RT_DEVICE_FLAG_RDWR);

    putenv("TZ=CST-8:00");
    tzset();
    return 0;
}
INIT_DEVICE_EXPORT(rt_hw_rtc_init);
