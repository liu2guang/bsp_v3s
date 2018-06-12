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

extern int sun4i_pwm_probe(uint32_t base, uint32_t clk);
int rt_hw_pwm_init(void)
{
    sun4i_pwm_probe(0x1c21400, 24000000);

    return 0;
}
INIT_DEVICE_EXPORT(rt_hw_pwm_init);

#ifdef RT_USING_FINSH
#include <finsh.h>
#include <msh.h>
int cmd_backlight(int argc, char** argv)
{
    int ps = 100;
    if (argc > 1) ps = atol(argv[1]);
    if (ps < 0) ps = 0;
    if (ps > 100) ps = 100;
    sun4i_pwm_disable(0);
    sun4i_pwm_config(0, ps*2000000L/100, 2000000L);
    sun4i_pwm_enable(0);
    rt_kprintf("backlight is %d%%\n", ps);

    return 0;
}

FINSH_FUNCTION_EXPORT_ALIAS(cmd_backlight, __cmd_backlight, Set Backlight With LCD.)
#endif //RT_USING_FINSH
