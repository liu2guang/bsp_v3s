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

#define UART_THR        0x00
#define UART_RHB        0x00
#define UART_IER        0X04
#define UART_IIR        0x08
#define UART_LSR        0x14
#define UART_MSR        0x18
#define UART_USR        0x7c
#define __REG(x)     (*((volatile ulong *)(x)))

struct hw_uart_device
{
    uint32_t base;
    char *name;
    int irqno;
    uint32_t pin[2];
    int32_t mode[2];
};

static void rt_hw_uart_isr(int vector, void *param)
{
    struct rt_serial_device *serial = (struct rt_serial_device *)param;
    struct hw_uart_device *uart = (struct hw_uart_device *)serial->parent.user_data;
    RT_ASSERT(uart != RT_NULL);

    volatile ulong data = 0;
    int ir = __REG(uart->base + UART_IIR)&0x0f;
    if (ir == 0x06){
        data = __REG(uart->base + UART_LSR);
    }else if (ir == 0x04 || ir == 0x0c){
        rt_hw_serial_isr(serial, RT_SERIAL_EVENT_RX_IND);
    }else if (ir == 0x00){
        data = __REG(uart->base + UART_MSR);
    }else if (ir == 0x02){
        data = __REG(uart->base + UART_IIR);
    }else if (ir == 0x07){
        data = __REG(uart->base + UART_USR);
    }
}

extern void NS16550_init(void* com_port, int baud_divisor);
extern int ns16550_calc_divisor(void* com_port, int clock, int baudrate);
static rt_err_t uart_configure (struct rt_serial_device *serial, struct serial_configure *cfg)
{
    struct hw_uart_device *uart = (struct hw_uart_device *)serial->parent.user_data;
    RT_ASSERT(uart != RT_NULL);

	int clock_divisor = ns16550_calc_divisor((void*)uart->base, 24000000, cfg->baud_rate);
	NS16550_init((void*)uart->base, clock_divisor);

    return RT_EOK;
}

static rt_err_t uart_control(struct rt_serial_device *serial, int cmd, void *arg)
{
    struct hw_uart_device *uart = (struct hw_uart_device *)serial->parent.user_data;
    RT_ASSERT(uart != RT_NULL);

    switch (cmd)
    {
    case RT_DEVICE_CTRL_CLR_INT:
        /* disable rx irq */
        __REG(uart->base + UART_IER) = 0x00;
        rt_hw_interrupt_mask(uart->irqno);
        break;

    case RT_DEVICE_CTRL_SET_INT:
        /* enable rx irq */
        __REG(uart->base + UART_IER) |= 0x01;
        rt_hw_interrupt_umask(uart->irqno);
        break;
    }

    return (RT_EOK);
}

static int uart_putc (struct rt_serial_device *serial, char c)
{
    struct hw_uart_device *uart = (struct hw_uart_device *)serial->parent.user_data;
    RT_ASSERT(uart != RT_NULL);

    while ((__REG(uart->base + UART_USR) & 0x02) == 0x00);
    __REG(uart->base + UART_THR) = c;
    return 1;
}

static int uart_getc (struct rt_serial_device *serial)
{
    struct hw_uart_device *uart = (struct hw_uart_device *)serial->parent.user_data;
    RT_ASSERT(uart != RT_NULL);

    int ch = -1;
    if ((__REG(uart->base + UART_USR) & 0x08) == 0x08)
        ch = (__REG(uart->base + UART_RHB) & 0xff);
    return ch;
}

const struct rt_uart_ops _uart_ops =
{
    uart_configure,
    uart_control,
    uart_putc,
    uart_getc,
};

static struct hw_uart_device _uart0_user =
{
    UART0_BASE,
    "uart",
    32,
    {SUNXI_GPB(8), SUNXI_GPB(9)},
    {PIN_TYPE(SUNXI_GPB_UART0)|PULL_UP, PIN_TYPE(SUNXI_GPB_UART0)|PULL_UP}
};
static struct rt_serial_device _uart0;

void uart_pin_config(struct rt_serial_device *dev, struct hw_uart_device *uart)
{
    int i;
    for (i=0; i<sizeof(uart->pin)/sizeof(uart->pin[0]); i++) gpio_set_mode(uart->pin[i], uart->mode[i]);
    rt_hw_interrupt_install(uart->irqno, rt_hw_uart_isr, dev, uart->name);
    rt_hw_interrupt_mask(uart->irqno);
    rt_hw_serial_register(dev, uart->name, RT_DEVICE_FLAG_RDWR | RT_DEVICE_FLAG_INT_RX, uart);
}

int rt_hw_uart_init(void)
{
    struct serial_configure config = RT_SERIAL_CONFIG_DEFAULT;

    _uart0.ops = &_uart_ops;
    _uart0.config = config;
    uart_pin_config(&_uart0, &_uart0_user);

    return 0;
}
INIT_BOARD_EXPORT(rt_hw_uart_init);