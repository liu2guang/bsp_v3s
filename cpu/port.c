#include <rthw.h>
#include <rtthread.h>
#include "armv7.h"

/**
 * This function will initialize thread stack
 *
 * @param tentry the entry of thread
 * @param parameter the parameter of entry
 * @param stack_addr the beginning stack address
 * @param texit the function will be called when thread exit
 *
 * @return stack address
 */
rt_uint8_t *rt_hw_stack_init(void *tentry, void *parameter,
	rt_uint8_t *stack_addr, void *texit)
{
	rt_uint32_t *stk;

	stk 	 = (rt_uint32_t*)stack_addr;
	*(stk) 	 = (rt_uint32_t)tentry;			/* entry point */
	*(--stk) = (rt_uint32_t)texit;			/* lr */
	*(--stk) = 0;							/* r12 */
	*(--stk) = 0;							/* r11 */
	*(--stk) = 0;							/* r10 */
	*(--stk) = 0;							/* r9 */
	*(--stk) = 0;							/* r8 */
	*(--stk) = 0;							/* r7 */
	*(--stk) = 0;							/* r6 */
	*(--stk) = 0;							/* r5 */
	*(--stk) = 0;							/* r4 */
	*(--stk) = 0;							/* r3 */
	*(--stk) = 0;							/* r2 */
	*(--stk) = 0;							/* r1 */
	*(--stk) = (rt_uint32_t)parameter;		/* r0 : argument */

	/* cpsr */
	if ((rt_uint32_t)tentry & 0x01)
		*(--stk) = SVCMODE | 0x20;			/* thumb mode */
	else
		*(--stk) = SVCMODE;					/* arm mode   */

	/* return task's current stack address */
	return (rt_uint8_t *)stk;
}

/** reset CPU */
void rt_hw_cpu_reset()
{
	rt_kprintf("Restarting system...\n");
	rt_hw_interrupt_disable();
    
    extern int cmd_reboot(int argc, char** argv); 
    
    cmd_reboot(1, RT_NULL); 
	while(1);
	/* NEVER REACHED */
}

/** shutdown CPU */
void rt_hw_cpu_shutdown()
{
	rt_kprintf("shutdown...\n");
	rt_hw_interrupt_disable();
	while(1);
	/* NEVER REACHED */
}