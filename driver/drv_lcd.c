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

struct lcdfb_device
{
    struct rt_device device; 
    struct rt_device_graphic_info info;
    int index;
    int bufsize;
    void* fb_address[2];
}; 

static rt_err_t _lcd_init(rt_device_t device)
{
    return RT_EOK; 
}

extern void sunxi_composer_fbbase_set(void* fbbase);
static rt_err_t _lcd_control(rt_device_t device, int cmd, void *args)
{
    struct lcdfb_device *lcdfb = (struct lcdfb_device *)device;
    RT_ASSERT(lcdfb != RT_NULL);

    switch(cmd)
    {
    case RTGRAPHIC_CTRL_GET_INFO:
        rt_memcpy(args, &lcdfb->info, sizeof(lcdfb->info)); 
        break;
    case RTGRAPHIC_CTRL_RECT_UPDATE:
        lcdfb->index = (lcdfb->index+1) % 2;
        rt_memcpy(lcdfb->fb_address[lcdfb->index], lcdfb->info.framebuffer, lcdfb->bufsize);
        sunxi_composer_fbbase_set(lcdfb->fb_address[lcdfb->index]);
        break;
    }

    return RT_EOK;
}

#ifdef RT_USING_I2C
enum {
	NS2009_LOW_POWER_READ_X		= 0xc0,
	NS2009_LOW_POWER_READ_Y		= 0xd0,
	NS2009_LOW_POWER_READ_Z1	= 0xe0,
	NS2009_LOW_POWER_READ_Z2	= 0xf0,
};

static void touch_thread_entry(void *parameter)
{
    struct lcdfb_device *lcd = (struct lcdfb_device*)parameter;
    int x = 0, y = 0, z1 = 0;
    const int i2c = 0, ns2009 = 0x48;

    while(1)
    {
        rt_uint8_t buf[2];
        int recv = 0;
        do{
            if(i2c_reg_readbuf(i2c, ns2009, NS2009_LOW_POWER_READ_Z1, buf, 2) != 2) break;
            z1 = (buf[0]<<4)|(buf[1]>>4);
            if(i2c_reg_readbuf(i2c, ns2009, NS2009_LOW_POWER_READ_X, buf, 2) != 2) break;
            x = (buf[0]<<4)|(buf[1]>>4);
            if(i2c_reg_readbuf(i2c, ns2009, NS2009_LOW_POWER_READ_Y, buf, 2) != 2) break;
            y = (buf[0]<<4)|(buf[1]>>4);
            rt_kprintf("recv:%d,%d,%d\n",x,y,z1);
            recv = 1;
        }while (0);

        rt_thread_delay(1000);
    }
}
#endif

static struct lcdfb_device lcd; 
extern void *video_hw_init_probe(void* base, rt_uint8_t *deep, rt_uint16_t* width, rt_uint16_t* height);
int rt_hw_lcd_init(void)
{
    lcd.index = 0;
    lcd.fb_address[0] = RT_HW_HEAP_END;
    lcd.fb_address[1] = lcd.fb_address[0] + 0x400000;
    rt_memset(lcd.fb_address[0], 0, 0x400000);
#if 0
    rt_memset(lcd.fb_address[0], 0xff, 0x400000);
    int i,j,color,size=4;
    for (i=1; i<479; i++){
        for (j=1; j<799; j++){
            if (i<120) color = 0xff;
            else if (i<240) color = 0xff00;
            else if (i<360) color = 0xff0000;
            else color = 0x00;
            rt_memcpy(lcd.fb_address[0]+(i*800+j)*size, &color, size);
        }
    }
#endif
#if 0
    rt_memset(lcd.fb_address[0], 0xff, 0x400000);
    int i,j,color,size=2;
    for (i=1; i<479; i++){
        for (j=1; j<799; j++){
            if (i<120) color = 0x1f;
            else if (i<240) color = 0x7e0;
            else if (i<360) color = 0xf800;
            else color = 0x00;
            rt_memcpy(lcd.fb_address[0]+(i*800+j)*size, &color, size);
        }
    }
#endif
    rt_memset(lcd.fb_address[1], 0, 0x400000);
    video_hw_init_probe(lcd.fb_address[0], &lcd.info.bits_per_pixel, &lcd.info.width, &lcd.info.height);
    lcd.info.pixel_format = (lcd.info.bits_per_pixel==16)?RTGRAPHIC_PIXEL_FORMAT_RGB565:RTGRAPHIC_PIXEL_FORMAT_RGB888;
    lcd.bufsize = RT_ALIGN(lcd.info.width * lcd.info.height * (lcd.info.bits_per_pixel==16?2:4), 64);
    lcd.info.framebuffer = rt_malloc(lcd.bufsize);

    lcd.device.type    = RT_Device_Class_Graphic; 
    lcd.device.init    = _lcd_init; 
    lcd.device.open    = RT_NULL;
    lcd.device.close   = RT_NULL;
    lcd.device.read    = RT_NULL;
    lcd.device.write   = RT_NULL;
    lcd.device.control = _lcd_control; 
    rt_device_register(&lcd.device, "lcd", RT_DEVICE_FLAG_RDWR); 

#ifdef RT_USING_I2C
    /* start touch */
    rt_thread_t tid = rt_thread_create("lcd_touch", touch_thread_entry,
                           &lcd,
                           2048, RT_THREAD_PRIORITY_MAX - 10, 20);
    rt_thread_startup(tid);
#endif

    return 0;
}
INIT_EXPORT(rt_hw_lcd_init, "5.end");
