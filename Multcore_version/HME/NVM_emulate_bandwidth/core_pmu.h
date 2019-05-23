/*
 *	Copyright (C) 2015-2016 Yizhou Shan <shanyizhou@ict.ac.cn>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License along
 *	with this program; if not, write to the Free Software Foundation, Inc.,
 *	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/types.h>
#include <linux/percpu.h>

static inline void core_pmu_cpuid(u32 *eax, u32 *ebx, u32 *ecx, u32 *edx)
{
	unsigned int op = *eax;
	asm volatile (
		"cpuid"
		: "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
		: "a"(op)
	);
}

static inline u64 core_pmu_rdtsc(void)
{
	u32 edx, eax;
	u64 retval = 0;

	asm volatile (
		"rdtsc"
		:"=a"(eax), "=d"(edx)
	);
	retval = (u64)edx << 32 | (u64)eax;
	return retval;
}

/*
 * It is deprecated to use self-defined assembly to touch MSRs
 * But .. leave them alone, would you?
 */

static inline u64 core_pmu_rdmsr(u32 addr)
{
	u32 edx, eax;
	u64 retval = 0;
	
	asm volatile (
		"rdmsr"
		:"=a"(eax), "=d"(edx)
		:"c"(addr)
	);
	retval = (u64)edx << 32 | (u64)eax;
	return retval;
}

static inline void core_pmu_wrmsr(u32 addr, u64 value)
{
	asm volatile (
		"wrmsr"
		:
		:"c"(addr), "d"((u32)(value>>32)), "a"((u32)value)
	);
}

/* General Core PMU API */
void core_pmu_show_msrs(void);
void core_pmu_clear_msrs(void);
void core_pmu_enable_counting(void);
void core_pmu_disable_counting(void);
void core_pmu_clear_ovf(void);
void core_pmu_enable_predefined_event(int event, u64 threshold);
void core_pmu_start_sampling(void);
void core_pmu_clear_counter(void);

int core_pmu_proc_create(void);
void core_pmu_proc_remove(void);

struct pre_event {
	int event;
	u64 threshold;
};

extern u64 pre_event_init_value;
DECLARE_PER_CPU(u64, PERCPU_NMI_TIMES);
