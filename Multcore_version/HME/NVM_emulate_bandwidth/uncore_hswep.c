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

#define pr_fmt(fmt) "UNCORE HSWEP: " fmt

/*
 * Support:
 * O	Platform:		Xeon E5 v3, Xeon E7 v3
 *	MicroArchitecture:	Haswell-EP, Haswell-EX
 *
 * For more information about Xeon E5 v3 and E7 v3 uncore PMU, plese consult
 * [Intel Xeon E5 and E7 v3 Family Uncore Performance Monitoring Reference Manual]
 */

/*
 * Ancient:
 * O	Platform:		Xeon E5 v2, Xeon E7 v2
 *	MicroArchitecture:	Ivy Bridge-EP, Ivy Bridge-EX
 *
 * O	Platform:		Xeon E5, Xeon E7
 *	MicroArchitecture:	Sandy Bridge-EP, Westmere-EX
 */

#include "uncore_pmu.h"

#include <asm/setup.h>

#include <linux/pci.h>
#include <linux/types.h>
#include <linux/kernel.h>

/* HSWEP MSR Box-Level Control Register Bit Layout */
#define HSWEP_MSR_BOX_CTL_RST_CTRL	(1 << 0)	/* Reset Control */
#define HSWEP_MSR_BOX_CTL_RST_CTRS	(1 << 1)	/* Reset Counters */
#define HSWEP_MSR_BOX_CTL_FRZ		(1 << 8)	/* Freeze all counters */
#define HSWEP_MSR_BOX_CTL_INIT		(HSWEP_MSR_BOX_CTL_RST_CTRL | \
					 HSWEP_MSR_BOX_CTL_RST_CTRS )

/* HSWEP MSR Counter-Level Control Register Bit Layout */
#define HSWEP_MSR_EVNTSEL_EVENT		0x000000FF	/* Event to counted */
#define HSWEP_MSR_EVNTSEL_UMASK		0x0000FF00	/* Subevent within the selected event */
#define HSWEP_MSR_EVNTSEL_RST		(1 << 17)	/* Reset/Clear this counter */
#define HSWEP_MSR_EVNTSEL_EDGE_DET	(1 << 18)	/* Hard to say... */
#define HSWEP_MSR_EVNTSEL_TID_EN	(1 << 19)	/* TID filter enable */
#define HSWEP_MSR_EVNTSEL_EN		(1 << 22)	/* Local counter enable */
#define HSWEP_MSR_EVNTSEL_INVERT	(1 << 23)	/* Invert comparision against threshold */
#define HSWEP_MSR_EVNTSEL_THRESHOLD	0xFF000000	/* Threshold used in counter comparison */
#define HSWEP_MSR_RAW_EVNTSEL_MASK	(HSWEP_MSR_EVNTSEL_EVENT	| \
					 HSWEP_MSR_EVNTSEL_UMASK	| \
					 HSWEP_MSR_EVNTSEL_EDGE_DET	| \
					 HSWEP_MSR_EVNTSEL_INVERT	| \
					 HSWEP_MSR_EVNTSEL_THRESHOLD)

/* HSWEP PCI Box-Level Control Register Bit Layout */
#define HSWEP_PCI_BOX_CTL_FRZ		HSWEP_MSR_BOX_CTL_FRZ
#define HSWEP_PCI_BOX_CTL_INIT		HSWEP_MSR_BOX_CTL_INIT

/* HSWEP PCI Counter-Level Control Register Bit Layout */
#define HSWEP_PCI_EVNTSEL_EVENT		0x000000FF	/* Event to counted */
#define HSWEP_PCI_EVNTSEL_UMASK		0x0000FF00	/* Subevent within the selected event */
#define HSWEP_PCI_EVNTSEL_RST		(1 << 17)	/* Reset/Clear this counter */
#define HSWEP_PCI_EVNTSEL_EDGE_DET	(1 << 18)	/* Hard to say... */
#define HSWEP_PCI_EVNTSEL_OV_EN		(1 << 20)	/* Enable overflow report */
#define HSWEP_PCI_EVNTSEL_EN		(1 << 22)	/* Local counter enable */
#define HSWEP_PCI_EVNTSEL_INVERT	(1 << 23)	/* Invert comparision against threshold */
#define HSWEP_PCI_EVNTSEL_THRESHOLD	0xFF000000	/* Threshold used in counter comparison */
#define HSWEP_PCI_RAW_EVNTSEL_MASK	(HSWEP_PCI_EVNTSEL_EVENT	| \
					 HSWEP_PCI_EVNTSEL_UMASK	| \
					 HSWEP_PCI_EVNTSEL_EDGE_DET	| \
					 HSWEP_PCI_EVNTSEL_OV_EN	| \
					 HSWEP_PCI_EVNTSEL_INVERT	| \
					 HSWEP_PCI_EVNTSEL_THRESHOLD)

/* HSWEP Uncore Global Per-Socket MSRs */
#define HSWEP_MSR_PMON_GLOBAL_CTL	0x700
#define HSWEP_MSR_PMON_GLOBAL_STATUS	0x701
#define HSWEP_MSR_PMON_GLOBAL_CONFIG	0x702

/* HSWEP Uncore U-box */
#define HSWEP_MSR_U_PMON_BOX_STATUS	0x708
#define HSWEP_MSR_U_PMON_UCLK_FIXED_CTL	0x703
#define HSWEP_MSR_U_PMON_UCLK_FIXED_CTR	0x704
#define HSWEP_MSR_U_PMON_EVNTSEL0	0x705
#define HSWEP_MSR_U_PMON_CTR0		0x709

/* HSWEP Uncore PCU-box */
#define HSWEP_MSR_PCU_PMON_BOX_CTL	0x710
#define HSWEP_MSR_PCU_PMON_BOX_FILTER	0x715
#define HSWEP_MSR_PCU_PMON_BOX_STATUS	0x716
#define HSWEP_MSR_PCU_PMON_EVNTSEL0	0x711
#define HSWEP_MSR_PCU_PMON_CTR0		0x717

/* HSWEP Uncore S-box */
#define HSWEP_MSR_S_PMON_BOX_CTL	0x720
#define HSWEP_MSR_S_PMON_BOX_STATUS	0x725
#define HSWEP_MSR_S_PMON_EVNTSEL0	0x721
#define HSWEP_MSR_S_PMON_CTR0		0x726
#define HSWEP_MSR_S_MSR_OFFSET		0xA

/* HSWEP Uncore C-box */
#define HSWEP_MSR_C_PMON_BOX_CTL	0xE00
#define HSWEP_MSR_C_PMON_BOX_FILTER0	0xE05
#define HSWEP_MSR_C_PMON_BOX_FILTER1	0xE06
#define HSWEP_MSR_C_PMON_BOX_STATUS	0xE07
#define HSWEP_MSR_C_PMON_EVNTSEL0	0xE01
#define HSWEP_MSR_C_PMON_CTR0		0xE08
#define HSWEP_MSR_C_MSR_OFFSET		0x10
#define HSWEP_MSR_C_EVENTSEL_MASK	(HSWEP_MSR_RAW_EVNTSEL_MASK | \
					 HSWEP_MSR_EVNTSEL_TID_EN)

/* HSWEP Uncore HA-box */
#define HSWEP_PCI_HA_PMON_BOX_STATUS	0xF8
#define HSWEP_PCI_HA_PMON_BOX_CTL	0xF4
#define HSWEP_PCI_HA_PMON_CTL0		0xD8
#define HSWEP_PCI_HA_PMON_CTR0		0xA0

//添加一个计数器和一个控制器
#define HSWEP_PCI_HA_PMON_CTL1		0xDC
#define HSWEP_PCI_HA_PMON_CTR1		0xA8


/* HSWEP Uncore IMC-box */
#define HSWEP_PCI_IMC_PMON_BOX_STATUS	0xF8
#define HSWEP_PCI_IMC_PMON_BOX_CTL	0xF4
#define HSWEP_PCI_IMC_PMON_CTL0		0xD8
#define HSWEP_PCI_IMC_PMON_CTR0		0xA0
#define HSWEP_PCI_IMC_PMON_FIXED_CTL	0xF0
#define HSWEP_PCI_IMC_PMON_FIXED_CTR	0xD0

/* HSWEP Uncore IRP-box */
#define HSWEP_PCI_IRP_PMON_BOX_STATUS	0xF8
#define HSWEP_PCI_IRP_PMON_BOX_CTL	0xF4

/* HSWEP Uncore QPI-box */
#define HSWEP_PCI_QPI_PMON_BOX_STATUS	0xF8
#define HSWEP_PCI_QPI_PMON_BOX_CTL	0xF4
#define HSWEP_PCI_QPI_PMON_CTL0		0xD8
#define HSWEP_PCI_QPI_PMON_CTR0		0xA0

/* HSWEP Uncore R2PCIE-box */
#define HSWEP_PCI_R2PCIE_PMON_BOX_STATUS 0xF8
#define HSWEP_PCI_R2PCIE_PMON_BOX_CTL	0xF4
#define HSWEP_PCI_R2PCIE_PMON_CTL0	0xD8
#define HSWEP_PCI_R2PCIE_PMON_CTR0	0xA0

/* HSWEP Uncore R3QPI-box */
#define HSWEP_PCI_R3QPI_PMON_BOX_STATUS	0xF8
#define HSWEP_PCI_R3QPI_PMON_BOX_CTL	0xF4
#define HSWEP_PCI_R3QPI_PMON_CTL0	0xD8
#define HSWEP_PCI_R3QPI_PMON_CTR0	0xA0

/******************************************************************************
 * MSR Type 
 *****************************************************************************/

static void hswep_uncore_msr_show_box(struct uncore_box *box)
{
	unsigned long long value;
	
	unsigned long long value1;

	pr_info("\033[034m---------------------- Show MSR Box ----------------------\033[0m");
	pr_info("MSR Box %d, on Node %d", box->idx, box->nodeid);

	rdmsrl(uncore_msr_box_ctl(box), value);
	pr_info("MSR Box-level Control: 0x%llx", value);

	rdmsrl(uncore_msr_box_status(box), value);
	pr_info("MSR Box-level Status:  0x%llx", value);

	if (box->event)																//这里show了event和计数器
		pr_info("... Current Event:     %s", box->event->desc);

	rdmsrl(uncore_msr_perf_ctl(box), value);	
	rdmsrl(uncore_msr_perf_ctl_1(box), value1);	                              //show里面带入了第二个ctl和ctr
	pr_info("... Control Register:  0x%llx & 0x%llx", value, value1);

	rdmsrl(uncore_msr_perf_ctr(box), value);
	rdmsrl(uncore_msr_perf_ctr_1(box), value1);
	pr_info("... Counter Register:  0x%llx & 0x%llx", value, value1);
}

static void hswep_uncore_msr_init_box(struct uncore_box *box)
{
	unsigned int msr;

	msr = uncore_msr_box_ctl(box);
	if (msr)
		wrmsrl(msr, HSWEP_MSR_BOX_CTL_INIT);
}

static void hswep_uncore_msr_enable_box(struct uncore_box *box)
{
	unsigned long long config;
	unsigned int msr;

	msr = uncore_msr_box_ctl(box);
	if (msr) {
		rdmsrl(msr, config);
		config &= ~HSWEP_MSR_BOX_CTL_FRZ;
		wrmsrl(msr, config);
	}
}

static void hswep_uncore_msr_disable_box(struct uncore_box *box)
{
	unsigned long long config;
	unsigned int msr;

	msr = uncore_msr_box_ctl(box);
	if (msr) {
		rdmsrl(msr, config);
		config |= HSWEP_MSR_BOX_CTL_FRZ;
		wrmsrl(msr, config);
	}
}

static void hswep_uncore_msr_enable_event(struct uncore_box *box,                 
					  struct uncore_event *event1,struct uncore_event *event2)
{
	wrmsrl(uncore_msr_perf_ctl(box), event1->disable);
	wrmsrl(uncore_msr_perf_ctl_1(box), event2->disable);
}

static void hswep_uncore_msr_disable_event(struct uncore_box *box,
					   struct uncore_event *event1,struct uncore_event *event2)
{
	wrmsrl(uncore_msr_perf_ctl(box), event1->enable);
	wrmsrl(uncore_msr_perf_ctl_1(box), event2->enable);
}

static void hswep_uncore_msr_write_counter(struct uncore_box *box, u64 value1,u64 value2)
{
	wrmsrl(uncore_msr_perf_ctr(box), value1 & uncore_box_ctr_mask(box));
	wrmsrl(uncore_msr_perf_ctr_1(box), value2 & uncore_box_ctr_mask(box));
}

static void hswep_uncore_msr_read_counter(struct uncore_box *box, u64 *value1,u64 *value2)
{
	u64 tmp1;
	u64 tmp2;
	
	rdmsrl(uncore_msr_perf_ctr(box), tmp1);
	*value1 = tmp1 & uncore_box_ctr_mask(box);
	
	rdmsrl(uncore_msr_perf_ctr_1(box), tmp2);
	*value2 = tmp2 & uncore_box_ctr_mask(box);
}
	


/*
 * Actually, some operations may differ among different box types. But we are
 * not building a mature perf system, emulating NVM is the only client for now,
 * so leave the holes. I thought I would not touch this code later. :)
 * 
 *实际上，某些操作可能因不同的框类型而不同。 
  但是我们没有建立一个成熟的perf系统，模拟NVM是目前唯一的客户端，所以留下漏洞。 我想我以后不会碰这个代码
  
 * If you want to go further, please read the PMU implementation code within
 * linux kernel. You can find different functions are applied to different boxes.
 * The x86 PMU part is in: arch/x86/kernel/cpu/perf*.c
 */
#define HSWEP_UNCORE_MSR_BOX_OPS()				\
	.show_box	= hswep_uncore_msr_show_box,		\
	.init_box	= hswep_uncore_msr_init_box,		\
	.clear_box	= hswep_uncore_msr_init_box,		\
	.enable_box	= hswep_uncore_msr_enable_box,		\
	.disable_box	= hswep_uncore_msr_disable_box,		\
	.enable_event	= hswep_uncore_msr_enable_event,	\
	.disable_event	= hswep_uncore_msr_disable_event,	\
	.write_counter	= hswep_uncore_msr_write_counter,	\
	.read_counter	= hswep_uncore_msr_read_counter		


const struct uncore_box_ops HSWEP_UNCORE_UBOX_OPS = {
	HSWEP_UNCORE_MSR_BOX_OPS()
};

const struct uncore_box_ops HSWEP_UNCORE_PCUBOX_OPS = {
	HSWEP_UNCORE_MSR_BOX_OPS()
};

const struct uncore_box_ops HSWEP_UNCORE_SBOX_OPS = {
	HSWEP_UNCORE_MSR_BOX_OPS()
};

const struct uncore_box_ops HSWEP_UNCORE_CBOX_OPS = {
	HSWEP_UNCORE_MSR_BOX_OPS()
};

struct uncore_box_type HSWEP_UNCORE_UBOX = {
	.name		= "U-BOX",
	.num_counters	= 2,
	.num_boxes	= 1,
	.perf_ctr_bits	= 48,
	.perf_ctr	= HSWEP_MSR_U_PMON_CTR0,
	.perf_ctl	= HSWEP_MSR_U_PMON_EVNTSEL0,
	.event_mask	= 0,
	.fixed_ctr_bits	= 48,
	.fixed_ctr	= HSWEP_MSR_U_PMON_UCLK_FIXED_CTR,
	.fixed_ctl	= HSWEP_MSR_U_PMON_UCLK_FIXED_CTL,
	.box_status	= HSWEP_MSR_U_PMON_BOX_STATUS,
	.ops		= &HSWEP_UNCORE_UBOX_OPS
};

struct uncore_box_type HSWEP_UNCORE_PCUBOX = {
	.name		= "PCU-BOX",
	.num_counters	= 4,
	.num_boxes	= 1,
	.perf_ctr_bits	= 48,
	.perf_ctr	= HSWEP_MSR_PCU_PMON_CTR0,
	.perf_ctl	= HSWEP_MSR_PCU_PMON_EVNTSEL0,
	.event_mask	= 0,
	.box_ctl	= HSWEP_MSR_PCU_PMON_BOX_CTL,
	.box_status	= HSWEP_MSR_PCU_PMON_BOX_STATUS,
	.box_filter0	= HSWEP_MSR_PCU_PMON_BOX_FILTER,
	.ops		= &HSWEP_UNCORE_PCUBOX_OPS
};

struct uncore_box_type HSWEP_UNCORE_SBOX = {
	.name		= "S-BOX",
	.num_counters	= 4,
	.num_boxes	= 4,
	.perf_ctr_bits	= 48,
	.perf_ctr	= HSWEP_MSR_S_PMON_CTR0,
	.perf_ctl	= HSWEP_MSR_S_PMON_EVNTSEL0,
	.event_mask	= 0,
	.box_ctl	= HSWEP_MSR_S_PMON_BOX_CTL,
	.box_status	= HSWEP_MSR_S_PMON_BOX_STATUS,
	.msr_offset	= HSWEP_MSR_S_MSR_OFFSET,
	.ops		= &HSWEP_UNCORE_SBOX_OPS
};

struct uncore_box_type HSWEP_UNCORE_CBOX = {
	.name		= "C-BOX",
	.num_counters	= 4,
	.num_boxes	= 18,
	.perf_ctr_bits	= 48,
	.perf_ctr	= HSWEP_MSR_C_PMON_CTR0,
	.perf_ctl	= HSWEP_MSR_C_PMON_EVNTSEL0,
	.event_mask	= HSWEP_MSR_C_EVENTSEL_MASK,
	.box_ctl	= HSWEP_MSR_C_PMON_BOX_CTL,
	.box_status	= HSWEP_MSR_C_PMON_BOX_STATUS,
	.box_filter0	= HSWEP_MSR_C_PMON_BOX_FILTER0,
	.box_filter1	= HSWEP_MSR_C_PMON_BOX_FILTER1,
	.msr_offset	= HSWEP_MSR_C_MSR_OFFSET,
	.ops		= &HSWEP_UNCORE_CBOX_OPS
};

enum {
	HSWEP_UNCORE_UBOX_ID	= UNCORE_MSR_UBOX_ID,
	HSWEP_UNCORE_PCUBOX_ID	= UNCORE_MSR_PCUBOX_ID,
	HSWEP_UNCORE_SBOX_ID	= UNCORE_MSR_SBOX_ID,
	HSWEP_UNCORE_CBOX_ID	= UNCORE_MSR_CBOX_ID
};

struct uncore_box_type *HSWEP_UNCORE_MSR_TYPE[] = {
	[HSWEP_UNCORE_UBOX_ID]   = &HSWEP_UNCORE_UBOX,
	[HSWEP_UNCORE_PCUBOX_ID] = &HSWEP_UNCORE_PCUBOX,
	[HSWEP_UNCORE_SBOX_ID]   = &HSWEP_UNCORE_SBOX,
	[HSWEP_UNCORE_CBOX_ID]   = &HSWEP_UNCORE_CBOX,
	NULL
};

/******************************************************************************
 * PCI Type 
 *****************************************************************************/

static void hswep_uncore_pci_show_box(struct uncore_box *box)
{
	struct pci_dev *pdev = box->pdev;
	unsigned int config, low, high;
	unsigned int config1, low1, high1;
	
	/* The same with some print functions... */
	pr_info("\033[034m---------------------- Show PCI Box ----------------------\033[0m");
	pr_info("PCI Box %d, on Node %d, %x:%x:%x, %d:%d:%d, Kref = %d",
		box->idx,
		box->nodeid,
		box->pdev->bus->number,
		box->pdev->vendor,
		box->pdev->device,
		box->pdev->bus->number,
		(box->pdev->devfn >> 3) & 0x1f,
		(box->pdev->devfn) & 0x7,
		box->pdev->dev.kobj.kref.refcount.refs.counter);

	pci_read_config_dword(pdev, uncore_pci_box_ctl(box), &config);
	pr_info("PCI Box-level Control: 0x%x", config);

	pci_read_config_dword(pdev, uncore_pci_box_status(box), &config);
	pr_info("PCI Box-level Status:  0x%x", config);

	if (box->event)
		pr_info("... Current Event:     %s", box->event->desc);

	pci_read_config_dword(pdev, uncore_pci_perf_ctl(box), &config);
	pci_read_config_dword(pdev, uncore_pci_perf_ctl_1(box), &config1);
	pr_info("... Control Register:  0x%x & 0x%x", config, config1);

	pci_read_config_dword(pdev, uncore_pci_perf_ctr(box), &low);
	pci_read_config_dword(pdev, uncore_pci_perf_ctr(box)+4, &high);
	pci_read_config_dword(pdev, uncore_pci_perf_ctr_1(box), &low1);
	pci_read_config_dword(pdev, uncore_pci_perf_ctr_1(box)+4, &high1);
	pr_info("... Counter Register:  0x%x<<32 | 0x%x ---> %Ld", high, low,
		((u64)high << 32) | (u64)low);
	pr_info("... Counter Register:  0x%x<<32 | 0x%x ---> %Ld", high1, low1,
		((u64)high1 << 32) | (u64)low1);		
}

static void hswep_uncore_pci_init_box(struct uncore_box *box)
{
	/* Clear all control and counter registers */
	pci_write_config_dword(box->pdev,
			       uncore_pci_box_ctl(box),
			       HSWEP_PCI_BOX_CTL_INIT);

	/* Write '1' will clear overflow bit */
	pci_write_config_dword(box->pdev,
			       uncore_pci_box_status(box),
			       0xf);
}

static void hswep_uncore_pci_enable_box(struct uncore_box *box)
{
	struct pci_dev *dev = box->pdev;
	unsigned int ctl = uncore_pci_box_ctl(box);
	unsigned int config = 0; 
	
	if (!pci_read_config_dword(dev, ctl, &config)) {
		/* Un-Freeze all counters */
		config &= ~HSWEP_PCI_BOX_CTL_FRZ;
		pci_write_config_dword(dev, ctl, config);
	}
}

static void hswep_uncore_pci_disable_box(struct uncore_box *box)
{
	struct pci_dev *dev = box->pdev;
	unsigned int ctl = uncore_pci_box_ctl(box);
	unsigned int config = 0; 
	
	if (!pci_read_config_dword(dev, ctl, &config)) {
		/* Freeze all counters */
		config |= HSWEP_PCI_BOX_CTL_FRZ;
		pci_write_config_dword(dev, ctl, config);
	}
}

//一个ctl enable一个事件 对应的那个count应该就开始记录了                 下面的都要改
static void hswep_uncore_pci_enable_event(struct uncore_box *box,
					  struct uncore_event *event1,struct uncore_event *event2)
{
	pci_write_config_dword(box->pdev,
			       uncore_pci_perf_ctl(box),
			       event1->enable);
	pci_write_config_dword(box->pdev,
			       uncore_pci_perf_ctl_1(box),
			       event2->enable);
}

static void hswep_uncore_pci_disable_event(struct uncore_box *box,
					   struct uncore_event *event1,struct uncore_event *event2)
{
	pci_write_config_dword(box->pdev,
			       uncore_pci_perf_ctl(box),
			       event1->disable);
	pci_write_config_dword(box->pdev,
			       uncore_pci_perf_ctl_1(box),
			       event2->disable);
}

static void hswep_uncore_pci_write_counter(struct uncore_box *box, u64 value1,u64 value2)
{
	u32 low1, high1;
	u32 low2, high2;
	
	low1 = (u32)(value1 & 0xffffffff);
	high1 = (u32)((value1 & uncore_box_ctr_mask(box)) >> 32);
	
	low2 = (u32)(value2 & 0xffffffff);
	high2 = (u32)((value2 & uncore_box_ctr_mask(box)) >> 32);

	pci_write_config_dword(box->pdev, uncore_pci_perf_ctr(box), low1);
	pci_write_config_dword(box->pdev, uncore_pci_perf_ctr(box)+4, high1);
	
	pci_write_config_dword(box->pdev, uncore_pci_perf_ctr_1(box), low2);
	pci_write_config_dword(box->pdev, uncore_pci_perf_ctr_1(box)+4, high2);
}

static void hswep_uncore_pci_read_counter(struct uncore_box *box, u64 *value1,u64 *value2)
{
	unsigned int low1, high1;
	unsigned int low2, high2;
	
	pci_read_config_dword(box->pdev, uncore_pci_perf_ctr(box), &low1);
	pci_read_config_dword(box->pdev, uncore_pci_perf_ctr(box)+4, &high1);
	
	pci_read_config_dword(box->pdev, uncore_pci_perf_ctr_1(box), &low2);
	pci_read_config_dword(box->pdev, uncore_pci_perf_ctr_1(box)+4, &high2);

	*value1 = ((u64)high1 << 32) | (u64)low1;
	*value1 &= uncore_box_ctr_mask(box);
	
	*value2 = ((u64)high2 << 32) | (u64)low2;
	*value2 &= uncore_box_ctr_mask(box);
}


#define HSWEP_UNCORE_PCI_BOX_OPS()				\
	.show_box	= hswep_uncore_pci_show_box,		\
	.init_box	= hswep_uncore_pci_init_box,		\
	.clear_box	= hswep_uncore_pci_init_box,		\
	.enable_box	= hswep_uncore_pci_enable_box,		\
	.disable_box	= hswep_uncore_pci_disable_box,		\
	.enable_event	= hswep_uncore_pci_enable_event,	\
	.disable_event	= hswep_uncore_pci_disable_event,	\
	.write_counter	= hswep_uncore_pci_write_counter,	\
	.read_counter	= hswep_uncore_pci_read_counter		
	
	
const struct uncore_box_ops HSWEP_UNCORE_HABOX_OPS = {
	HSWEP_UNCORE_PCI_BOX_OPS()
};

const struct uncore_box_ops HSWEP_UNCORE_IMCBOX_OPS = {
	HSWEP_UNCORE_PCI_BOX_OPS()
};

const struct uncore_box_ops HSWEP_UNCORE_IRPBOX_OPS = {
	HSWEP_UNCORE_PCI_BOX_OPS()
};

const struct uncore_box_ops HSWEP_UNCORE_QPIBOX_OPS = {
	HSWEP_UNCORE_PCI_BOX_OPS()
};

const struct uncore_box_ops HSWEP_UNCORE_R2PCIEBOX_OPS = {
	HSWEP_UNCORE_PCI_BOX_OPS()
};

const struct uncore_box_ops HSWEP_UNCORE_R3QPIBOX_OPS = {
	HSWEP_UNCORE_PCI_BOX_OPS()
};

struct uncore_box_type HSWEP_UNCORE_HA = {
	.name		= "HA-Box",
	.num_counters	= 5,
	.num_boxes	= 2,
	.perf_ctr_bits  = 48,
	.perf_ctr	= HSWEP_PCI_HA_PMON_CTR0,         //这里给的定义
	.perf_ctl	= HSWEP_PCI_HA_PMON_CTL0,
	
	//补充定义
	.perf_ctr_1 = HSWEP_PCI_HA_PMON_CTR1,
	.perf_ctl_1 = HSWEP_PCI_HA_PMON_CTL1,
	
	
	.event_mask	= 0,
	.box_ctl	= HSWEP_PCI_HA_PMON_BOX_CTL,
	.box_status	= HSWEP_PCI_HA_PMON_BOX_STATUS,
	.ops		= &HSWEP_UNCORE_HABOX_OPS
};

struct uncore_box_type HSWEP_UNCORE_IMC = {
	.name		= "IMC-Box",
	.num_counters	= 5,
	.num_boxes	= 8,
	.perf_ctr_bits	= 48,
	.perf_ctr	= HSWEP_PCI_IMC_PMON_CTR0,
	.perf_ctl	= HSWEP_PCI_IMC_PMON_CTL0,
	.event_mask	= 0,
	.fixed_ctr_bits	= 48,
	.fixed_ctr	= HSWEP_PCI_IMC_PMON_FIXED_CTR,
	.fixed_ctl	= HSWEP_PCI_IMC_PMON_FIXED_CTL,
	.box_ctl	= HSWEP_PCI_IMC_PMON_BOX_CTL,
	.box_status	= HSWEP_PCI_IMC_PMON_BOX_STATUS,
	.ops		= &HSWEP_UNCORE_IMCBOX_OPS
};

struct uncore_box_type HSWEP_UNCORE_IRP = {
	.name		= "IRP-Box",
	.num_counters	= 4,
	.num_boxes	= 1,
	.box_ctl	= HSWEP_PCI_IRP_PMON_BOX_CTL,
	.box_status	= HSWEP_PCI_IRP_PMON_BOX_STATUS,
	.ops		= &HSWEP_UNCORE_IRPBOX_OPS
};

struct uncore_box_type HSWEP_UNCORE_QPI = {
	.name		= "QPI-Box",
	.num_counters	= 4,
	.num_boxes	= 3,
	.perf_ctr_bits	= 48,
	.perf_ctr	= HSWEP_PCI_QPI_PMON_CTR0,
	.perf_ctl	= HSWEP_PCI_QPI_PMON_CTL0,
	.event_mask	= 0,
	.box_ctl	= HSWEP_PCI_QPI_PMON_BOX_CTL,
	.box_status	= HSWEP_PCI_QPI_PMON_BOX_STATUS,
	.ops		= &HSWEP_UNCORE_QPIBOX_OPS
};

struct uncore_box_type HSWEP_UNCORE_R2PCIE = {
	.name		= "R2PCIE-Box",
	.num_counters	= 4,
	.num_boxes	= 1,
	.perf_ctr_bits	= 48,
	.perf_ctr	= HSWEP_PCI_R2PCIE_PMON_CTR0,
	.perf_ctl	= HSWEP_PCI_R2PCIE_PMON_CTL0,
	.event_mask	= 0,
	.box_ctl	= HSWEP_PCI_R2PCIE_PMON_BOX_CTL,
	.box_status	= HSWEP_PCI_R2PCIE_PMON_BOX_STATUS,
	.ops		= &HSWEP_UNCORE_R2PCIEBOX_OPS
};

struct uncore_box_type HSWEP_UNCORE_R3QPI = {
	.name		= "R3QPI-Box",
	.num_counters	= 3,
	.num_boxes	= 3,
	.perf_ctr_bits	= 48,
	.perf_ctr	= HSWEP_PCI_R3QPI_PMON_CTR0,
	.perf_ctl	= HSWEP_PCI_R3QPI_PMON_CTL0,
	.event_mask	= 0,
	.box_ctl	= HSWEP_PCI_R3QPI_PMON_BOX_CTL,
	.box_status	= HSWEP_PCI_R3QPI_PMON_BOX_STATUS,
	.ops		= &HSWEP_UNCORE_R3QPIBOX_OPS
};

enum {
	HSWEP_UNCORE_PCI_HA_ID		= UNCORE_PCI_HA_ID,
	HSWEP_UNCORE_PCI_IMC_ID		= UNCORE_PCI_IMC_ID,
	HSWEP_UNCORE_PCI_IRP_ID		= UNCORE_PCI_IRP_ID,
	HSWEP_UNCORE_PCI_QPI_ID		= UNCORE_PCI_QPI_ID,
	HSWEP_UNCORE_PCI_R2PCIE_ID	= UNCORE_PCI_R2PCIE_ID,
	HSWEP_UNCORE_PCI_R3QPI_ID	= UNCORE_PCI_R3QPI_ID
};

struct uncore_box_type *HSWEP_UNCORE_PCI_TYPE[] = {
	[HSWEP_UNCORE_PCI_HA_ID]     = &HSWEP_UNCORE_HA,
	[HSWEP_UNCORE_PCI_IMC_ID]    = &HSWEP_UNCORE_IMC,
	[HSWEP_UNCORE_PCI_IRP_ID]    = &HSWEP_UNCORE_IRP,
	[HSWEP_UNCORE_PCI_QPI_ID]    = &HSWEP_UNCORE_QPI,
	[HSWEP_UNCORE_PCI_R2PCIE_ID] = &HSWEP_UNCORE_R2PCIE,
	[HSWEP_UNCORE_PCI_R3QPI_ID]  = &HSWEP_UNCORE_R3QPI,
	NULL
};

static const struct pci_device_id HSWEP_UNCORE_PCI_IDS[] = {
	{ /* Home Agent 0 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F30),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_HA_ID, 0),
	},
	{ /* Home Agent 1 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F38),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_HA_ID, 1),
	},
	{ /* MC0 Channel 0 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FB0),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IMC_ID, 0),
	},
	{ /* MC0 Channel 1 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FB1),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IMC_ID, 1),
	},
	{ /* MC0 Channel 2 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FB4),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IMC_ID, 2),
	},
	{ /* MC0 Channel 3 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FB5),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IMC_ID, 3),
	},
	{ /* MC1 Channel 0 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FD0),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IMC_ID, 4),
	},
	{ /* MC1 Channel 1 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FD1),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IMC_ID, 5),
	},
	{ /* MC1 Channel 2 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FD4),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IMC_ID, 6),
	},
	{ /* MC1 Channel 3 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FD5),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IMC_ID, 7),
	},
	{ /* IRP */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F39),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_IRP_ID, 0),
	},
	{ /* QPI0 Port 0 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F32),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_QPI_ID, 0),
	},
	{ /* QPI0 Port 1 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F33),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_QPI_ID, 1),
	},
	{ /* QPI1 Port 2 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F3a),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_QPI_ID, 2),
	},
	{ /* R2PCIe */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F34),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_R2PCIE_ID, 0),
	},
	{ /* R3QPI0 Link 0 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F36),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_R3QPI_ID, 0),
	},
	{ /* R3QPI0 Link 1 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F37),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_R3QPI_ID, 1),
	},
	{ /* R3QPI1 Link 2 */
		PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2F3E),
		.driver_data = UNCORE_PCI_DEV_DATA(HSWEP_UNCORE_PCI_R3QPI_ID, 2),
	},
	{ /* All Zero ;) */
		PCI_DEVICE(0, 0),
		.driver_data = 0,
	}
};

/**
 * hswep_pcibus_to_nodeid
 * @devid:	The device id of PCI UBOX device
 * Return:	Non-zero on failure
 *
 * Get the configuration mapping between PCI bus number and NUMA node ID.
 * Consult specific processor's datasheet for how to build the map.
 * After all, why not Intel just store a direct mapping array?
 */
static int hswep_pcibus_to_nodeid(int devid)
{
	struct pci_dev *ubox = NULL;
	unsigned int nodeid;
	int err, mapping, bus, i;

	while (1) {
		ubox = pci_get_device(PCI_VENDOR_ID_INTEL, devid, ubox);
		if (!ubox)
			break;
		bus = ubox->bus->number;

		/* Read Node ID Configuration Resgister */
		err = pci_read_config_dword(ubox, 0x40, &nodeid);
		if (err)
			break;

		/* Read Node ID Mapping Register */
		err = pci_read_config_dword(ubox, 0x54, &mapping);
		if (err)
			break;
		
		/* Every 3-bit maps a node */
		for (i = 0; i < 8; i++) {
			if (nodeid == ((mapping >> (i * 3)) & 0x7)) {
				uncore_pcibus_to_nodeid[bus] = i;
				break;
			}
		}
	}

	pci_dev_put(ubox);
	
	return err? pcibios_err_to_errno(err) : 0;
}

int hswep_cpu_init(void)
{
	if (HSWEP_UNCORE_CBOX.num_boxes > boot_cpu_data.x86_max_cores)
		HSWEP_UNCORE_CBOX.num_boxes = boot_cpu_data.x86_max_cores;

	uncore_msr_type = HSWEP_UNCORE_MSR_TYPE;
	
	/* Init the global uncore_pmu structure */
	uncore_pmu.name			= "Intel Xeon E5 v3 Uncore PMU";
	uncore_pmu.msr_type		= HSWEP_UNCORE_MSR_TYPE;
	uncore_pmu.global_ctl		= HSWEP_MSR_PMON_GLOBAL_CTL;
	uncore_pmu.global_status	= HSWEP_MSR_PMON_GLOBAL_STATUS;
	uncore_pmu.global_config	= HSWEP_MSR_PMON_GLOBAL_CONFIG;

	return 0;
}

static struct pci_driver HSWEP_UNCORE_PCI_DRIVER = {
	.name		= "E5-v3-UNCORE",
	.id_table	= HSWEP_UNCORE_PCI_IDS
};

int hswep_pci_init(void)
{
	int ret;
	
	ret = hswep_pcibus_to_nodeid(0x2F1E);
	if (ret)
		return ret;

	uncore_pci_driver	= &HSWEP_UNCORE_PCI_DRIVER;
	uncore_pci_type		= HSWEP_UNCORE_PCI_TYPE;

	uncore_pmu.pci_type	= HSWEP_UNCORE_PCI_TYPE;

	return 0;
}

/******************************************************************************
 * PMU Monitoring Events
 *
 * Note that: Users can set particuliar events using specific code/mask. The
 * following defined events are documented here because they could be used in
 * NVM emulation. I know all these event structure sucks. :(
 *****************************************************************************/

/*
 * Home Agent Events:	REQUESTS
 * Event Code: 0x01
 * Max. Inc/Cyc: 1
 * Register Restrictions: 0-3
 *
 * Counts the total number of read requests made into the Home Agent. Reads
 * include all read opcodes (including RFO). Writes include all writes (
 * streaming, evictions, HitM, etc).
 */

/*
 * LOCAL_READS: This filter includes only read requests coming from the local
 * socket. This is a good proxy for LLC Read Misses (including RFOs) from the
 * local socket.
 */
struct uncore_event ha_requests_local_reads = {
	.enable = (1<<22) | (1<<20) | 0x0100 | 0x0001,
	.disable = 0,
	.desc = "Read requests coming from the local socket"
};

/*
 * REMOTE_READS: This filter includes only read requests coming from remote
 * sockets. This is a good proxy for LLC Read Misses (including RFOs) from
 * remote sockets.
 */
struct uncore_event ha_requests_remote_reads = {
	.enable = (1<<22) | (1<<20) | 0x0200 | 0x0001,
	.disable = 0,
	.desc = "Read requests coming from remote sockets"
};

/*
 * READS: Incoming read requests. This is a good proxy for LLC Read Misses (
 * including RFOs).
 */
struct uncore_event ha_requests_reads = {
	.enable = (1<<22) | (1<<20) | 0x0300 | 0x0001,
	.disable = 0,
	.desc = "Incoming read requests total"
};

/*
 * LOCAL_WRITES: This filter includes only writes coming from the local socket.
 */
struct uncore_event ha_requests_local_writes = {
	.enable = (1<<22) | (1<<20) | 0x0400 | 0x0001,
	.disable = 0,
	.desc = "Write requests from local socket"
};

/*
 * REMOTE_WRITES: This filter includes only writes coming from remote sockets.
 */
struct uncore_event ha_requests_remote_writes = {
	.enable = (1<<22) | (1<<20) | 0x0800 | 0x0001,
	.disable = 0,
	.desc = "Write requests from remote socket"
};

/*
 * WRITES: Incoming write requests.
 */
struct uncore_event ha_requests_writes = {
	.enable = (1<<22) | (1<<20) | 0x0B00 | 0x0001,
	.disable = 0,
	.desc = "Incoming write requests total"
};

/*
 * Home Agent Events:	IMC_READS
 * Event Code: 0x17
 * Max. Inc/Cyc: 4
 * Register Restrictions: 0-3
 *
 * Count of the number of reads issued to any of the memory controller channels.
 * This can be filtered by the priority of the reads. Note that, this event does
 * not count reads the bypass path. That is counted separately in HA_IMC.BYPASS.
 */
struct uncore_event ha_imc_reads = {
	.enable = (1<<22) | (1<<20) | 0x0100 | 0x0017,
	.disable = 0,
	.desc = "HA to IMC normal priority read requests"
};

/*
 * Home Agent Events:	IMC_WRITES
 * Event Code: 0x1A
 * Max. Inc/Cyc: 1
 * Register Restrictions: 0-3
 *
 * Count the total number of full line writes issued from the HA into the memory
 * controller. This counts for all four channels. It can be filtered by full/partial
 * and ISOCH/non-ISOCH.
 */
struct uncore_event ha_imc_writes_full = {
	.enable = (1<<22) | (1<<20) | 0x0100 | 0x001A,
	.disable = 0,
	.desc = "HA to IMC full-line Non-ISOCH write"
};

struct uncore_event ha_imc_writes_partial = {
	.enable = (1<<22) | (1<<20) | 0x0200 | 0x001A,
	.disable = 0,
	.desc = "HA to IMC partial-line Non-ISOCH write"
};

/******************************************************************************
 * Integrated Memory Controller (IMC) Part
 *
 * Note that: This section should NOT be included here. It should be separated
 * like uncore_imc_hswep.c, it is a sublayer of uncore_imc.
 *****************************************************************************/

/*
 * (The same PCI devices as IMC PMON)
 *
 * IMC0, Channel 0-1 --> 20:0 20:1 (2fb4 2fb5)
 * IMC1, Channel 2-3 --> 21:0 21:1 (2fb0 2fb1)
 *
 * IMC1, Channel 2-3 --> 23:0 23:1 (2fd0 2fd1)
 */

static const struct pci_device_id HSWEP_E5_IMC[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FB4), },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FB5), },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FB0), },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FB1), },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FD0), },
	{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, 0x2FD1), },
	{ 0, }
};

static const struct pci_device_id HSWEX_E7_IMC[] = {
	{ 0, }
};

/*
 * This test wants to use [dimm_temp_thrt_lmt_[0:2]] to throttle
 * bandwidth. After some tests, it turns out that: THRT_CRIT and
 * THRT_MID has very very little impact on bandwidth(critical only?).
 * So, what do they mean? what is critical or middle transactions?
 *
 * THRT_HI can throttle the memory bandwidth. But it is weird.
 */
__always_unused static void __test2(struct pci_dev *pdev)
{
	unsigned int config, offset;
	int i;

	for (i = 0; i < 3; i++) {
		offset = 0x130 + 4 * i;
		pci_read_config_dword(pdev, offset, &config);
		config = 0x00ffff;
		pci_write_config_dword(pdev, offset, config);
	}
	pci_read_config_dword(pdev, 0x134, &config);
	pr_info("%x", config);
}

/*
 * This test wants to use [chn_tmp_cfg] to throttle bandwidth.
 * According to datasheet, some fields seems like be able to
 * throttle the bandwidth. After some tests, it turns out this
 * register has *no* impact on bandwidth.
 */
__always_unused static void __test3(struct pci_dev *pdev)
{
	unsigned int config;

	pci_read_config_dword(pdev, 0x108, &config);
	config |= 0x3ff;
	config |= 0x0f0000;
	pci_write_config_dword(pdev, 0x108, config);
	pci_read_config_dword(pdev, 0x108, &config);
	pr_info("%x", config);
}

/*
 * Use [thrt_pwr_dimm_[0:2]].THRT_PWR to throttle bandwidth.
 * Bit 11:0, default value after hardware reset: 0xfff
 * Seriously Yizhou, you should learn more about MC/DRAM! :(
 */
static int hswep_imc_set_threshold(struct pci_dev *pdev, unsigned int threshold)
{
	u32 offset, i;
	u16 config;
	
	/* 3 DIMMs Per Channel are populated at most */
	for (i = 0; i < 3; i++) {
		offset = 0x190 + 2 * i;
		
		pci_read_config_word(pdev, offset, &config);
		config &= (1 << 15);
		
		/* XXX Relationship???? */
		switch (threshold) {
			case 2: /* 1/2 */
				config |= 0x00ff;
				break;
			case 4: /* 1/4 */
				config |= 0x007f;
				break;
			default:
				config |= 0x0fff;
		}
		pci_write_config_word(pdev, offset, config);
	}

	return 0;
}

/*
 * Use [thrt_pwr_dimm_[0:2]].THRT_PER_EN bit to enable throttling
 * Bit 15:15, default value after hardware reset: 0x1 (Enable)
 */
static int hswep_imc_enable_throttle(struct pci_dev *pdev)
{
	u32 offset, i;
	u16 config;

	/* 3 DIMMs Per Channel are populated at most */
	for (i = 0; i < 3; i++) {
		offset = 0x190 + 2 * i;
		pci_read_config_word(pdev, offset, &config);
		config |= (1 << 15);
		pci_write_config_word(pdev, offset, config);
	}
	return 0;
}

static void hswep_imc_disable_throttle(struct pci_dev *pdev)
{
	u32 offset, i;
	u16 config;

	/* 3 DIMMs Per Channel are populated at most */
	for (i = 0; i < 3; i++) {
		offset = 0x190 + 2 * i;
		pci_read_config_word(pdev, offset, &config);
		config &= ~(1 << 15);
		pci_write_config_word(pdev, offset, config);
	}
}

static const struct uncore_imc_ops HSWEP_E5_IMC_OPS = {
	.set_threshold		= hswep_imc_set_threshold,
	.enable_throttle	= hswep_imc_enable_throttle,
	.disable_throttle	= hswep_imc_disable_throttle
};

int hswep_imc_init(void)
{
	uncore_imc_device_ids = HSWEP_E5_IMC;
	uncore_imc_ops = &HSWEP_E5_IMC_OPS;

	return 0;
}
