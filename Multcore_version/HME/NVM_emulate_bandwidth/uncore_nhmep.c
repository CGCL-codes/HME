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

/*
 * Platform:
 *	Xeon 5600 serials	Westmere
 *	Xeon 5500 serials	Nehalem
 *
 * One caveat:
 *  1) This code is some kind dangerous. We are *not* supposed to access MSR
 *     directly, we should use the API provided by kernel, which ensures some
 *     sort of consistency. Anyway, live with it.
 *
 * Two notes:
 *  1) This is the first version of using pmu to emulate nvm, all this code is
 *     binded to Nehalem-EP, and is not bridged to the current uncore_pmu layer.
 *  2) This module is a self-contained one, you can use it to sample specific
 *     events. Of course, i guess no one will do that. :) Use perf instead.
 */

#include <asm/nmi.h>

#include <linux/pci.h>
#include <linux/smp.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/string.h>
#include <linux/hrtimer.h>

#define __AC(X,Y)	(X##Y)

/*
 * Architectual MSRs
 * And their control bits
 */

#define __MSR_IA32_PMC0			0x0C1
#define __MSR_IA32_PERFEVTSEL0		0x186
#define __MSR_CORE_PERF_GLOBAL_STATUS	0x38E
#define __MSR_CORE_PERF_GLOBAL_CTRL	0x38F
#define __MSR_IA32_MISC_ENABLE		0x1a0
#define __MSR_IA32_DEBUG_CTL		0x1d9
#define __MSR_IA32_PERFMON_ENABLE	(__AC(1, ULL)<<7)
#define __MSR_IA32_ENABLE_UNCORE_PMI	(__AC(1, ULL)<<13)

/*
 * o Nehalem PMC Bit Width
 * o Nehalem PMC Maximum Value Mask
 */

#define NHM_UNCORE_PMC_BIT_WIDTH	48
#define NHM_UNCORE_PMC_VALUE_MASK	((__AC(1, ULL)<<48) - 1)

/*
 * o Nehalem Global Control Registers
 * o Nehalem Performance Counter and Control Registers
 */

#define NHM_UNCORE_GLOBAL_CTRL		0x391	/* Read/Write */
#define NHM_UNCORE_GLOBAL_STATUS	0x392	/* Read-Only */
#define NHM_UNCORE_GLOBAL_OVF_CTRL	0x393	/* Write-Only */
#define NHM_UNCORE_PMCO			0x3b0	/* Read/Write */
#define NHM_UNCORE_PMC1			0x3b1	/* Read/Write */
#define NHM_UNCORE_PMC2			0x3b2	/* Read/Write */
#define NHM_UNCORE_PMC3			0x3b3	/* Read/Write */
#define NHM_UNCORE_PMC4			0x3b4	/* Read/Write */
#define NHM_UNCORE_PMC5			0x3b5	/* Read/Write */
#define NHM_UNCORE_PMC6			0x3b6	/* Read/Write */
#define NHM_UNCORE_PMC7			0x3b7	/* Read/Write */
#define NHM_UNCORE_PERFEVTSEL0		0x3c0	/* Read/Write */
#define NHM_UNCORE_PERFEVTSEL1		0x3c1	/* Read/Write */
#define NHM_UNCORE_PERFEVTSEL2		0x3c2	/* Read/Write */
#define NHM_UNCORE_PERFEVTSEL3		0x3c3	/* Read/Write */
#define NHM_UNCORE_PERFEVTSEL4		0x3c4	/* Read/Write */
#define NHM_UNCORE_PERFEVTSEL5		0x3c5	/* Read/Write */
#define NHM_UNCORE_PERFEVTSEL6		0x3c6	/* Read/Write */
#define NHM_UNCORE_PERFEVTSEL7		0x3c7	/* Read/Write */

#define NHM_UNCORE_PMC_BASE		NHM_UNCORE_PMCO
#define NHM_UNCORE_SEL_BASE		NHM_UNCORE_PERFEVTSEL0

/*
 * Control Bit in PERFEVTSEL
 * EVENT_MASK, UNIT_MASK, PMI_ENABLE, COUNT_ENABLE
 * are used when manipulating PEREVTSELx.
 */

#define NHM_PEREVTSEL_EVENT_MASK		(__AC(0xff, ULL))
#define NHM_PEREVTSEL_UNIT_MASK			(__AC(0xff, ULL)<<8)
#define NHM_PEREVTSEL_OCC_CTR_RST		(__AC(1, ULL)<<17)
#define NHM_PEREVTSEL_EDGE_DETECT		(__AC(1, ULL)<<18)
#define NHM_PEREVTSEL_PMI_ENABLE		(__AC(1, ULL)<<20)
#define NHM_PEREVTSEL_COUNT_ENABLE		(__AC(1, ULL)<<22)
#define NHM_PEREVTSEL_INVERT			(__AC(1, ULL)<<23)

/*
 * Control Bit in GLOBAL_CTRL
 * When EN_FRZ is set, all counters will stop
 * counting when one counter overflows. Counting
 * will be restarted by setting each EN_PMCx bit.
 */

#define NHM_UNCORE_GLOBAL_CTRL_EN_PMC0		(__AC(1, ULL)<<0)
#define NHM_UNCORE_GLOBAL_CTRL_EN_PMC1		(__AC(1, ULL)<<1)
#define NHM_UNCORE_GLOBAL_CTRL_EN_PMC2		(__AC(1, ULL)<<2)
#define NHM_UNCORE_GLOBAL_CTRL_EN_PMC3		(__AC(1, ULL)<<3)
#define NHM_UNCORE_GLOBAL_CTRL_EN_PMC4		(__AC(1, ULL)<<4)
#define NHM_UNCORE_GLOBAL_CTRL_EN_PMC5		(__AC(1, ULL)<<5)
#define NHM_UNCORE_GLOBAL_CTRL_EN_PMC6		(__AC(1, ULL)<<6)
#define NHM_UNCORE_GLOBAL_CTRL_EN_PMC7		(__AC(1, ULL)<<7)
#define NHM_UNCORE_GLOBAL_CTRL_EN_PMI_CORE0	(__AC(1, ULL)<<48)
#define NHM_UNCORE_GLOBAL_CTRL_EN_PMI_CORE1	(__AC(1, ULL)<<49)
#define NHM_UNCORE_GLOBAL_CTRL_EN_PMI_CORE2	(__AC(1, ULL)<<50)
#define NHM_UNCORE_GLOBAL_CTRL_EN_PMI_CORE3	(__AC(1, ULL)<<51)
#define NHM_UNCORE_GLOBAL_CTRL_EN_FRZ		(__AC(1, ULL)<<63)

/*
 * Test result shows the EN_PMI_COREx bit
 * enables _one_ _physical_ core to receive a
 * PMI, which means _two_ _logical_ cores will
 * receive PMI.
 *
 * Also, if one package has more than four physical
 * cores, like Xeon 5600 which has 6, only 4 of 6
 * can receive PMI.
 */
#define NHM_UNCORE_GLOBAL_CTRL_EN_PMI		\
	(NHM_UNCORE_GLOBAL_CTRL_EN_PMI_CORE0 |	\
	 NHM_UNCORE_GLOBAL_CTRL_EN_PMI_CORE1 |	\
	 NHM_UNCORE_GLOBAL_CTRL_EN_PMI_CORE2 |	\
	 NHM_UNCORE_GLOBAL_CTRL_EN_PMI_CORE3 )

/*
 * Control Bit in GLOBAL_CTRL and OVF_CTRL
 */

#define NHM_UNCORE_GLOBAL_OVF_FC0	(__AC(1, ULL)<<32)
#define NHM_UNCORE_GLOBAL_OVF_PMI	(__AC(1, ULL)<<61)
#define NHM_UNCORE_GLOBAL_OVF_CHG	(__AC(1, ULL)<<63)

/*
 * Masks For Three GLOBAL MSRs.
 * Used when we wanna read from or write to these three MSRs. Masks help to
 * avoid writing reserved bit in MSR which can bring kernel panic.
 */

#define NHM_UNCORE_PMC_MASK			\
	(NHM_UNCORE_GLOBAL_CTRL_EN_PMC0 |	\
	 NHM_UNCORE_GLOBAL_CTRL_EN_PMC1 |	\
	 NHM_UNCORE_GLOBAL_CTRL_EN_PMC2 |	\
	 NHM_UNCORE_GLOBAL_CTRL_EN_PMC3 |	\
	 NHM_UNCORE_GLOBAL_CTRL_EN_PMC4 |	\
	 NHM_UNCORE_GLOBAL_CTRL_EN_PMC5 |	\
	 NHM_UNCORE_GLOBAL_CTRL_EN_PMC6 |	\
	 NHM_UNCORE_GLOBAL_CTRL_EN_PMC7 )

#define NHM_UNCORE_GLOBAL_CTRL_MASK		\
	(NHM_UNCORE_PMC_MASK | NHM_UNCORE_GLOBAL_CTRL_EN_PMI)

#define NHM_UNCORE_GLOBAL_STATUS_MASK		\
	(NHM_UNCORE_PMC_MASK		|	\
	 NHM_UNCORE_GLOBAL_OVF_PMI	|	\
	 NHM_UNCORE_GLOBAL_OVF_CHG  )

#define NHM_UNCORE_GLOBAL_OVF_CTRL_MASK	\
	(NHM_UNCORE_GLOBAL_STATUS_MASK | NHM_UNCORE_GLOBAL_OVF_FC0)

/*
 * Each PMCx and PERFEVTSELx forms a pair.
 * When we manipulate PMU, we usually handle a PMC/SEL pair.
 * Hence, in the code below, we operate PMCx and PERFEVTSELx
 * through their corresponding pair id.
 */
enum NHM_UNCORE_PMC_PAIR_ID {
	PMC_PID0,
	PMC_PID1,
	PMC_PID2,
	PMC_PID3,
	PMC_PID4,
	PMC_PID5,
	PMC_PID6,
	PMC_PID7,
	
	PMC_PID_MAX
};

/**
 * for_each_pmc_pair - loop each pmc pair in NHM PMU
 * @id:  pair id
 * @pmc: pmc msr address
 * @sel: perfevtsel msr address
 */
#define for_each_pmc_pair(id, pmc, sel) \
	for ((id) = 0, (pmc) = NHM_UNCORE_PMC_BASE, (sel) = NHM_UNCORE_SEL_BASE; \
		(id) < PMC_PID_MAX; (id)++, (pmc)++, (sel)++)


//#################################################
// NHM UNCORE EVENT
//#################################################

enum nhm_uncore_event_id {
	nhm_qhl_request_ioh_reads	=	1,
	nhm_qhl_request_ioh_writes	=	2,
	nhm_qhl_request_remote_reads	=	3,
	nhm_qhl_request_remote_writes	=	4,
	nhm_qhl_request_local_reads	=	5,
	nhm_qhl_request_local_writes	=	6,
	nhm_qmc_normal_reads_any	=	7,
	nhm_qmc_writes_full_any		=	8,
	nhm_qmc_writes_partial_any	=	9,

	NHM_UNCORE_EVENT_ID_MAX
};

const char *EVENT_DESC[NHM_UNCORE_EVENT_ID_MAX] = {
	"BLANK"
	"Read requests from the IOH",
	"Write requests from the IOH",
	"Read requests from a remote socket",
	"Write requests from a remote socket",
	"Read requests from the local socket",
	"Write requests from the local socket",
	"Quickpath Memory Controller read requests",
	"Full cache line writes to DRAM",
	"Partial cache line writes to DRAM"
};

static u64 nhm_uncore_event_map[NHM_UNCORE_EVENT_ID_MAX] = 
{
	[nhm_qhl_request_ioh_reads]	=	0x0120,
	[nhm_qhl_request_ioh_writes]	=	0x0220,
	[nhm_qhl_request_remote_reads]	=	0x0420,
	[nhm_qhl_request_remote_writes]	=	0x0820,
	[nhm_qhl_request_local_reads]	=	0x1020,
	[nhm_qhl_request_local_writes]	=	0x2020,
	[nhm_qmc_normal_reads_any]	=	0x072c,
	[nhm_qmc_writes_full_any]	=	0x072f,
	[nhm_qmc_writes_partial_any]	=	0x382f,
};


//#################################################
//  ASSEMBLY PART
//#################################################

static void uncore_cpuid(u32 *eax, u32 *ebx, u32 *ecx, u32 *edx)
{
	u32 op = *eax;
	asm volatile("cpuid"
		: "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
		: "a"(op)
	);
}

static u64 uncore_rdtsc(void)
{
	u32 edx, eax;
	u64 retval = 0;

	asm volatile("rdtsc"
		:"=a"(eax), "=d"(edx)
	);
	retval = (u64)edx << 32 | (u64)eax;
	return retval;
}

static u64 uncore_rdmsr(u32 addr)
{
	u32 edx, eax;
	u64 retval = 0;
	
	asm volatile("rdmsr"
		:"=a"(eax), "=d"(edx)
		:"c"(addr)
	);
	retval = (u64)edx << 32 | (u64)eax;
	return retval;
}

static void uncore_wrmsr(u32 addr, u64 value)
{
	asm volatile("wrmsr"
		: :"c"(addr), "d"((u32)(value>>32)), "a"((u32)value)
	);
}

//#################################################
// CPU INFO PART
//#################################################

static u32  PERF_VERSION;
static u64  CPU_BASE_FREQUENCY;
static char CPU_BRAND[48];

static void cpu_brand_info(void)
{
	char *s;
	int i;
	u32 eax, ebx, ecx, edx;

	eax = 0x80000000;
	uncore_cpuid(&eax, &ebx, &ecx, &edx);

	if (eax < 0x80000004U)
		printk(KERN_INFO"CPUID Extended Function Not Supported.\n");
	
	s = CPU_BRAND;
	for (i = 0; i < 3; i++) {
		eax = 0x80000002U + i;
		uncore_cpuid(&eax, &ebx, &ecx, &edx);
		memcpy(s, &eax, 4); s += 4;
		memcpy(s, &ebx, 4); s += 4;
		memcpy(s, &ecx, 4); s += 4;
		memcpy(s, &edx, 4); s += 4;
	}
	printk(KERN_INFO "PMU %s\n", CPU_BRAND);
	
	/* FIXME */
	CPU_BASE_FREQUENCY = 2400000000ULL;
}


static void cpu_version_info(void)
{
	u32 eax, ebx, ecx, edx;
	u32 fid, model, sid;
	
	eax = 0x01;
	uncore_cpuid(&eax, &ebx, &ecx, &edx);
	
	/* If Family ID = 0xF, then use Extended Family ID
	   If Family ID = 0xF or 0x6, then use Extended Model ID */
	fid = (eax & 0xf00) >> 8;
	model = (eax & 0xf0) >> 4;
	sid = eax & 0xf;

	if (fid == 0xf || fid == 0x6)
		model |= ((eax & 0xf0000) >> 12);
	if (fid == 0xf)
		fid += ((eax & 0xff00000) >> 20);
	
	printk(KERN_INFO "PMU Intel Family ID=%x, Model=%x, Stepping ID=%d\n", fid, model, sid);
}

static void cpu_perf_info(void)
{
	u32 msr;
	u32 eax, ebx, ecx, edx;
	
	eax = 0x0a;
	uncore_cpuid(&eax, &ebx, &ecx, &edx);
	
	PERF_VERSION = eax & 0xff;
	printk(KERN_INFO "PMU CPU_PMU Version ID: %u\n", PERF_VERSION);
	
	/*
	 * The PERFMON_ENABLE bit is Read-Only, every
	 * core in one package has the same value.
	 * Therefore, it is sufficient to check one core.
	 */
	msr = uncore_rdmsr(__MSR_IA32_MISC_ENABLE);
	if (!(msr & __MSR_IA32_PERFMON_ENABLE)) {
		printk(KERN_INFO"PMU ERROR! CPU_PMU Disabled!\n");
	}
}

static void uncore_cpu_info(void)
{
	cpu_brand_info();
	cpu_version_info();
	cpu_perf_info();
}

//#################################################
//	NHM_UNCORE_PMU PART
//#################################################

static void nhm_uncore_show_msrs(void *info)
{
	int id, this_cpu;
	u64 pmc, sel, a, b;
	char *banner = "<---SHOW--->";
	
	this_cpu = get_cpu();
	a = uncore_rdmsr(NHM_UNCORE_GLOBAL_CTRL);
	b = uncore_rdmsr(NHM_UNCORE_GLOBAL_STATUS);
	printk(KERN_INFO "PMU %s in CPU %2d\n", banner, this_cpu);
	printk(KERN_INFO "PMU %s GLOBAL_CTRL = 0x%llx\n", banner, a);
	printk(KERN_INFO "PMU %s GLOBAL_STATUS = 0x%llx\n", banner, b);

	for_each_pmc_pair(id, pmc, sel) {
		a = uncore_rdmsr(pmc);
		b = uncore_rdmsr(sel);
		if (b) {/* active */
			printk(KERN_INFO "PMU %s PMC%d = %-20lld, %12llx SEL%d = 0x%llx\n",
				banner, id, a, a, id, b);
		}
	}
	printk(KERN_INFO"PMU\n");
	put_cpu();
}

static void nhm_uncore_show_all(void)
{
	printk(KERN_INFO "PMU SHOW NODE 0");
	smp_call_function_single(0, nhm_uncore_show_msrs, NULL, 1);
	printk(KERN_INFO "PMU SHOW NODE 1");
	smp_call_function_single(6, nhm_uncore_show_msrs, NULL, 1);
}

/**
 * Clear bit @pmcmask(overflow status) in GLOBAL_STATUS
 * Write into OVF_CTRL to clear GLOBAL_STATUS 
 */
static inline void nhm_uncore_clear_ovf(u64 pmcmask)
{
	u64 mask = pmcmask;
	mask |= (NHM_UNCORE_GLOBAL_OVF_PMI | NHM_UNCORE_GLOBAL_OVF_CHG);
	uncore_wrmsr(NHM_UNCORE_GLOBAL_OVF_CTRL, mask);
}

/**
 * Clear entire GLOBAL_STATUS.
 * Write into OVF_CTRL to clear GLOBAL_STATUS 
 */
static inline void nhm_uncore_clear_status(void)
{
	uncore_wrmsr(NHM_UNCORE_GLOBAL_OVF_CTRL, NHM_UNCORE_GLOBAL_OVF_CTRL_MASK);
}

static inline void nhm_uncore_clear_ctrl(void)
{
	uncore_wrmsr(NHM_UNCORE_GLOBAL_CTRL, 0);
}

static inline void nhm_uncore_clear_msrs(void *info)
{
	int id;
	u64 pmc, sel;

	nhm_uncore_clear_ctrl();
	nhm_uncore_clear_status();
	
	for_each_pmc_pair(id, pmc, sel) {
		uncore_wrmsr(pmc, 0);
		uncore_wrmsr(sel, 0);
	}
}

static inline void nhm_uncore_clear_all(void)
{
	smp_call_function_single(0, nhm_uncore_clear_msrs, NULL, 1);
	smp_call_function_single(6, nhm_uncore_clear_msrs, NULL, 1);
}

/**
 * nhm_uncore_set_event - Set event in pmc.
 * @pmcid: 	The id of the pmc pair
 * @event:	pre-defined event id
 * @pmcval:	the initial value in pmc
 */
static inline void nhm_uncore_set_event(int pmcid, int event, u64 pmcval)
{
	u64 selval;
	
	/* ok ok ok, this is a very very very safty mask!
	   Writing to reserved bits in MSR cause CPU to
	   generate #GP fault. #GP handler will make you die. */
	pmcval &= NHM_UNCORE_PMC_VALUE_MASK;
	selval = nhm_uncore_event_map[event] |
		 NHM_PEREVTSEL_COUNT_ENABLE  |
		 NHM_PEREVTSEL_PMI_ENABLE    ;

	uncore_wrmsr(pmcid + NHM_UNCORE_PMC_BASE, pmcval);
	uncore_wrmsr(pmcid + NHM_UNCORE_SEL_BASE, selval);
}

/**
 * nhm_uncore_enable_counting - enable counting
 * @frz:	stop counting when pmc overflows
 */
static inline void nhm_uncore_enable_counting(int frz)
{
	u64 mask;
	
	if (frz) {
		mask |= NHM_UNCORE_GLOBAL_CTRL_EN_FRZ;
	}
	
	mask |= NHM_UNCORE_PMC_MASK;
	mask |= NHM_UNCORE_GLOBAL_CTRL_EN_PMI;
	uncore_wrmsr(NHM_UNCORE_GLOBAL_CTRL, mask);
}

static inline void nhm_uncore_disable_counting(void)
{
	uncore_wrmsr(NHM_UNCORE_GLOBAL_CTRL, 0);
}


//#################################################
//	NMI_HANDLER PART
//#################################################

/*
 * Wrappers for remote read msr operation.
 * Add '__' as prefix to avoid conficting
 * with system defined struct name.
 */
struct __msr {
	u32 addr;
	u64 value;
}__attribute__((packed));

/*
 * SOME USEFUL VARIABLES.
 * The survivor cpu is the only _online_ cpu in Node 1. And XYZ cpu is the
 * receiver in Node 0 of remote call to read or set pmu.
 */
DEFINE_PER_CPU(int, OVF_COUNT);
static struct __msr m;
static int NMI_REGISTED;
static int FREEZE;
static int PMC;
static int SURVIVOR;
static int XYZ;
static u64 INITVAL;

static void __enable_pmi(void *info)
{
	u64 msr;
	msr = uncore_rdmsr(__MSR_IA32_DEBUG_CTL);
	if (!(msr & __MSR_IA32_ENABLE_UNCORE_PMI)) {
		msr |= __MSR_IA32_ENABLE_UNCORE_PMI;
		uncore_wrmsr(__MSR_IA32_DEBUG_CTL, msr);
	}
}

static void __lapic_init(void *info)
{
	apic_write(APIC_LVTPC, APIC_DM_NMI);
}

/**
 * Enable every online cpus to receive the
 * uncore pmi generated when pmc overflow.
 */
static void nhm_uncore_pmi_enable(void)
{
	int cpu;
	for_each_online_cpu(cpu) {
		smp_call_function_single(cpu, __enable_pmi, NULL, 1);
		smp_call_function_single(cpu, __lapic_init, NULL, 1);
	}
}

static void __remote_rdmsr(void *info)
{
	u32 edx, eax;
	struct __msr *mm;

	if (!info)
		return;

	mm = (struct __msr *)info;
	asm volatile("rdmsr"
		:"=a"(eax), "=d"(edx)
		:"c"(mm->addr)
	);
	mm->value = (u64)edx << 32 | (u64)eax;
}

static void __remote_restart(void *info)
{
	u64 ovfpmc = *(u64 *)info;
	nhm_uncore_clear_ovf(ovfpmc);
	nhm_uncore_set_event(PMC, nhm_qhl_request_remote_writes, INITVAL);
	nhm_uncore_enable_counting(FREEZE);
	
}

#define _UNCORE_DEBUG_

int nhm_uncore_nmi_handler(unsigned int type, struct pt_regs *regs)
{
	int this_cpu;
	u64 status, ovfpmc;

	this_cpu = get_cpu();
	printk(KERN_INFO "PMU NMI catched on CPU%d", this_cpu);
	if (this_cpu == SURVIVOR) {
		m.addr = NHM_UNCORE_GLOBAL_STATUS;
		smp_call_function_single(XYZ, __remote_rdmsr, &m, 1);
		status = m.value;
	} else {
		status = uncore_rdmsr(NHM_UNCORE_GLOBAL_STATUS);
	}

	ovfpmc = status & NHM_UNCORE_PMC_MASK;
	if (!ovfpmc)
		return NMI_DONE;

#ifdef _UNCORE_DEBUG_
	printk(KERN_INFO "PMU <---NMI CATCHED---> CPU %2d, STATUS=%llx\n",
		this_cpu, status);
#endif
	/*
	if (this_cpu == SURVIVOR) {
		smp_call_function_single(XYZ, __remote_restart, &ovfpmc, 1);
		goto done;
	}
	*/
	//nhm_uncore_clear_ovf(ovfpmc);
	//nhm_uncore_set_event(PMC, nhm_qhl_request_local_writes, INITVAL);
	//nhm_uncore_set_event(PMC, nhm_qhl_request_local_reads, INITVAL);
	//nhm_uncore_enable_counting(FREEZE);
	
done:
	this_cpu_inc(OVF_COUNT);
	put_cpu();
	return NMI_HANDLED;
}

static void uncore_nmi_register(void)
{
	register_nmi_handler(NMI_LOCAL, nhm_uncore_nmi_handler, NMI_FLAG_FIRST, "NHM_UNCORE_HANLDER");
	NMI_REGISTED = 1;
}

static void uncore_nmi_unregister(void)
{
	if (NMI_REGISTED) {
		unregister_nmi_handler(NMI_LOCAL, "NHM_UNCORE_HANLDER");
	}
}

//#################################################
// HRTIMER PART
//#################################################

static struct hrtimer UNCORE_PMU_TIMER;
static int TIMER_INITED;
static u64 DELAY_JIFFIES;
static u64 INTERVAL_NS;
static u64 WRITE_LATENCY_DELTA;

static void uncore_pmu_delay(void *ns)
{
	unsigned long usec;
	usec = (*(unsigned long *)ns)/1000;
	udelay(usec);
}

static inline void uncore_pmu_delay_cpu(int cpu, unsigned long ns)
{
	smp_call_function_single(cpu, uncore_pmu_delay, &ns, 0);
}

static inline u64 writes_to_delay(u64 nr_writes)
{
	return nr_writes * WRITE_LATENCY_DELTA;
}

static enum hrtimer_restart uncore_pmu_hrtimer_cb(struct hrtimer *hrtimer)
{
	u64 remote_writes, delay_ns;
	
	DELAY_JIFFIES++;
	remote_writes = uncore_rdmsr(NHM_UNCORE_PMCO);
	delay_ns = writes_to_delay(remote_writes);
	//delay_ns=0;
	uncore_pmu_delay_cpu(SURVIVOR, delay_ns);
	uncore_wrmsr(NHM_UNCORE_PMCO, 0);
	//printk(KERN_INFO "val=%20llu tsc=%20llu\n", remote_writes, uncore_rdtsc());
	
	hrtimer_forward_now(hrtimer, ns_to_ktime(INTERVAL_NS));
	return HRTIMER_RESTART;
}

static void uncore_pmu_hrtimer_init(void)
{
	hrtimer_init(&UNCORE_PMU_TIMER, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	UNCORE_PMU_TIMER.function = uncore_pmu_hrtimer_cb;
	TIMER_INITED = 1;
}

static void uncore_pmu_hrtimer_start(ktime_t time)
{
	if (TIMER_INITED)
		hrtimer_start(&UNCORE_PMU_TIMER, time, HRTIMER_MODE_REL);
}

static void uncore_pmu_hrtimer_cancel(void)
{
	if (TIMER_INITED)
		hrtimer_cancel(&UNCORE_PMU_TIMER);
}


//#################################################
// GENERAL MODULE PART
//#################################################

#define uncore_start(frz) \
	nhm_uncore_enable_counting(frz);

#define uncore_end() \
	nhm_uncore_disable_counting();

#define uncore_set_event(pmcid, event, pmcval) \
	nhm_uncore_set_event(pmcid, event, pmcval)

#define uncore_pmi_enable() \
	nhm_uncore_pmi_enable()

#define show()		nhm_uncore_show_msrs(NULL)
#define clear()		nhm_uncore_clear_msrs(NULL)
#define show_all()	nhm_uncore_show_all()
#define clear_all()	nhm_uncore_clear_all()

static void uncore_pmu_main(void)
{
	TIMER_INITED	= 0;			/* Hrtimer */
	NMI_REGISTED	= 0;			/* NMI Handler */
	SURVIVOR	= 6;			/* Node1, CPU 6. Sender.*/
	XYZ		= 0;			/* Node0, CPU 0. Receiver. */
	INITVAL		= -100;			/* Samping initial value */
	FREEZE		= 0;			/* Freeze after overflow */
	PMC		= PMC_PID0;		/* Used Counter */
	
	INTERVAL_NS	= 1*1000000;		/* Poll interval */
	WRITE_LATENCY_DELTA = 150;		/* PCM_WRITE - DRAM_WRITE */

	//uncore_pmi_enable();
	//uncore_nmi_register();

	uncore_pmu_hrtimer_init();

	clear_all();
	uncore_set_event(PMC, nhm_qhl_request_remote_writes, INITVAL);
	uncore_start(FREEZE);
	show_all();
	uncore_pmu_hrtimer_start(ktime_set(0, 1));
}

const char BEYBANNER[] = "PMU --------> EXIT <--------";
const char WELBANNER[] = "PMU <-------- INIT -------->";

void _read(void *info)
{
	u64 val1 = uncore_rdmsr(__MSR_CORE_PERF_GLOBAL_CTRL);
	u64 val2 = uncore_rdmsr(__MSR_IA32_PERFEVTSEL0);
	printk(KERN_INFO"CPU%2d, ctrl=%llx sel=%llx\n",
		smp_processor_id(), val1, val2);
}

void read(void)
{
	int cpu;
	for_each_online_cpu(cpu) {
		smp_call_function_single(cpu, _read, NULL, 1);
	}
}

/*
 * CAVEAT:
 * use:	numactl --physcpubind=0 /bin/bash
 * before insmod, to make sure that, everything
 * runs on CPU0. Nothing will impact benchmark.
 *
 * We put benchmark on CPU6 and the memory allocation
 * policy is only from Node0.
 */
static int __init uncore_pmu_init(void)
{
	int this_cpu;

	this_cpu = get_cpu();
	printk(KERN_INFO "%s ON CPU %d\n", WELBANNER, this_cpu);
	read();
	uncore_cpu_info();
	uncore_pmu_main();
	put_cpu();
	return 0;
}

static void uncore_pmu_exit(void)
{
	int this_cpu;

	this_cpu = get_cpu();
	show_all(); clear_all();
	uncore_nmi_unregister();
	uncore_pmu_hrtimer_cancel();
	printk(KERN_INFO "PMU DELAY_JIFFIES = %10lld\n", DELAY_JIFFIES);
	printk(KERN_INFO "%s ON CPU %2d\n", BEYBANNER, this_cpu);
	put_cpu();
}

module_init(uncore_pmu_init);
module_exit(uncore_pmu_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("shanyizhou@ict.ac.cn");
