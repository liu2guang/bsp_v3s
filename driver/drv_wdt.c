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

struct wdt_reset
{
    uint32_t base;
    void *dev_ptr;
    int msecs;
};

rt_err_t _wdt_init(rt_watchdog_t *wdt)
{
    return RT_EOK;
}

extern int sunxi_wdt_start(void *sunxi_wdt, int timeout);
extern int sunxi_wdt_ping(void *sunxi_wdt);
extern int sunxi_wdt_stop(void *sunxi_wdt);
rt_err_t _wdt_control(rt_watchdog_t *wdt, int cmd, void *arg)
{
    struct wdt_reset *param = (struct wdt_reset *)wdt->parent.user_data;
    RT_ASSERT(param != RT_NULL);

    switch (cmd)
    {
        case RT_DEVICE_CTRL_WDT_SET_TIMEOUT:{
            int msecs = *(int *)arg * 1000;
            if(msecs < 1000) msecs = 1000;
            else if(msecs > 16000) msecs = 16000;
            param->msecs = msecs;
            break;}
        case RT_DEVICE_CTRL_WDT_START:
            sunxi_wdt_start(param->dev_ptr, param->msecs/1000);
            break;
        case RT_DEVICE_CTRL_WDT_STOP:
            sunxi_wdt_stop(param->dev_ptr);
            break;
        case RT_DEVICE_CTRL_WDT_KEEPALIVE:
            sunxi_wdt_ping(param->dev_ptr);
            break;
        default:
            break;
    }
    return RT_EOK;
}

const struct rt_watchdog_ops _wdt_ops =
{
    _wdt_init,
    _wdt_control
};

static struct wdt_reset _wdt_param;
static struct rt_watchdog_device _wdt_device;
extern void* sunxi_wdt_probe(uint32_t base);
int rt_hw_wdt_init(void)
{
    _wdt_param.base = 0x1c20ca0;
    _wdt_param.msecs = 1000;
    _wdt_param.dev_ptr = sunxi_wdt_probe(_wdt_param.base);
    _wdt_device.ops = &_wdt_ops;
    rt_hw_watchdog_register(&_wdt_device, "wdt", RT_DEVICE_FLAG_RDWR, &_wdt_param);

    return 0;
}
INIT_DEVICE_EXPORT(rt_hw_wdt_init);
