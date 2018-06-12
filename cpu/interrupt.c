/*
 * File      : interrupt.c
 * This file is part of RT-Thread RTOS
 * COPYRIGHT (C) 2013-2014, RT-Thread Development Team
 *
 * The license and distribution terms for this file may be
 * found in the file LICENSE in this distribution or at
 * http://www.rt-thread.org/license/LICENSE
 *
 * Change Logs:
 * Date           Author       Notes
 * 2013-07-06     Bernard      first version
 * 2014-04-03     Grissiom     port to VMM
 */

#include <rthw.h>
#include <rtthread.h>
#include <interrupt.h>
#include <gic.h>

/* exception and interrupt handler table */
#define MAX_HANDLERS 160
struct rt_irq_desc isr_table[MAX_HANDLERS];
extern volatile rt_uint8_t rt_interrupt_nest;
rt_uint32_t rt_interrupt_from_thread;
rt_uint32_t rt_interrupt_to_thread;
rt_uint32_t rt_thread_switch_interrupt_flag;

void enable_interrupt(uint32_t irq_id, uint32_t cpu_id, uint32_t priority)
{
    gic_set_irq_priority(irq_id, priority);
    gic_set_irq_security(irq_id, false);    // set IRQ as non-secure
    gic_set_cpu_target(irq_id, cpu_id, true);
    gic_enable_irq(irq_id, true);
}

void disable_interrupt(uint32_t irq_id, uint32_t cpu_id)
{
    gic_enable_irq(irq_id, false);
    gic_set_cpu_target(irq_id, cpu_id, false);
}

/**
 * This function will initialize hardware interrupt
 */
void rt_hw_interrupt_init(void)
{
    gic_init();

    /* init exceptions table */
    rt_memset(isr_table, 0x00, sizeof(isr_table));

    /* init interrupt nest, and context in thread sp */
    rt_interrupt_nest = 0;
    rt_interrupt_from_thread = 0;
    rt_interrupt_to_thread = 0;
    rt_thread_switch_interrupt_flag = 0;
}

/**
 * This function will mask a interrupt.
 * @param vector the interrupt number
 */
void rt_hw_interrupt_mask(int vector)
{
    disable_interrupt(vector, 0);
}

/**
 * This function will un-mask a interrupt.
 * @param vector the interrupt number
 */
void rt_hw_interrupt_umask(int vector)
{
    enable_interrupt(vector, 0, 0);
}

/**
 * This function will install a interrupt service routine to a interrupt.
 * @param vector the interrupt number
 * @param new_handler the interrupt service routine to be installed
 * @param old_handler the old interrupt service routine
 */
rt_isr_handler_t rt_hw_interrupt_install(int vector, rt_isr_handler_t handler,
        void *param, char *name)
{
    rt_isr_handler_t old_handler = RT_NULL;

    if (vector < MAX_HANDLERS)
    {
        old_handler = isr_table[vector].handler;

        if (handler != RT_NULL)
        {
#ifdef RT_USING_INTERRUPT_INFO
            rt_strncpy(isr_table[vector].name, name, RT_NAME_MAX);
            isr_table[vector].counter = 0;
#endif /* RT_USING_INTERRUPT_INFO */
            isr_table[vector].handler = handler;
            isr_table[vector].param = param;
        }
        // arm_gic_set_cpu(0, vector, 1 << rt_cpu_get_smp_id());
    }

    return old_handler;
}

#ifdef RT_USING_FINSH
void list_irq(void)
{
    int irq;
    for (irq = 0; irq < MAX_HANDLERS; irq++)
    {
        if (!isr_table[irq].handler) continue;
#ifdef RT_USING_INTERRUPT_INFO
        rt_kprintf("nr:%4d, name: %*.s, count: %8d, handler: 0x%p, param: 0x%08x\r\n",
                irq, RT_NAME_MAX, isr_table[irq].name, isr_table[irq].counter,
                isr_table[irq].handler, isr_table[irq].param);
#else
        rt_kprintf("nr:%4d, handler: 0x%p, param: 0x%08x\r\n",
                irq, isr_table[irq].handler, isr_table[irq].param);
#endif
    }
}

#include <finsh.h>
FINSH_FUNCTION_EXPORT(list_irq, list system irq);
MSH_CMD_EXPORT(list_irq, list system irq);
#endif
