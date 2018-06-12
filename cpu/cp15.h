#ifndef __CP15_H__
#define __CP15_H__

void rt_cpu_mmu_disable(void);
void rt_cpu_mmu_enable(void);
void rt_cpu_tlb_set(volatile unsigned long*);
void rt_cpu_align_check_disable(void);
uint32_t get_arm_private_peripheral_base(void);

#endif
