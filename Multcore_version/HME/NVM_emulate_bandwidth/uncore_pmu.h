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
 * This file describes data structures and APIs of Uncore PMU programming.
 */

#ifndef pr_fmt
#define pr_fmt(fmt) "UNCORE_PMU: " fmt
#endif

#include <linux/pci.h>
#include <linux/types.h>
#include <linux/hrtimer.h>
#include <linux/compiler.h>

#define UNCORE_PMU_HRTIMER_INTERVAL     (60 * NSEC_PER_SEC)

#define UNCORE_MAX_SOCKET		8

/* PCI Driver Data <--> Box Type and IDX */
#define UNCORE_PCI_DEV_DATA(type, idx)	(((type) << 8) | (idx))
#define UNCORE_PCI_DEV_TYPE(data)	(((data) >> 8) & 0xFF)
#define UNCORE_PCI_DEV_IDX(data)	((data) & 0xFF)

/* PCI Type Array Index */
enum {
	UNCORE_PCI_HA_ID,
	UNCORE_PCI_IMC_ID,
	UNCORE_PCI_IRP_ID,
	UNCORE_PCI_QPI_ID,
	UNCORE_PCI_R2PCIE_ID,
	UNCORE_PCI_R3QPI_ID,
};

/* MSR Type Array Index */
enum {
	UNCORE_MSR_UBOX_ID,
	UNCORE_MSR_PCUBOX_ID,
	UNCORE_MSR_SBOX_ID,
	UNCORE_MSR_CBOX_ID
};

struct uncore_box_type;

/**
 * struct uncore_event
 * @enable:	Bit mask to enable this event
 * @disable:	Bis mask to disable this event
 * @desc:	Description about this event
 * @next:	Pointer to next event
 */
struct uncore_event {
	u64			enable;
	u64			disable;
	const char		*desc;
	struct list_head	next;
};

/**
 * struct uncore_box
 * @idx:		Index of this box
 * @nodeid:		NUMA node id of this box
 * @hrtimer_duration:	Duration of hrtimer
 * @hrtimer:		hrtimer to poll the box
 * @event:		Currently counting or sampling event
 * @box_type:		Pointer to the type of this box
 * @pdev:		PCI device of this box (For PCI type box)
 * @next:		List of the same type boxes
 *
 * Describe a single uncore pmu box instance. All boxes of the same type
 * are linked together. Since all boxes of all nodes all mixed together,
 * hence node_id is needed to distinguish two boxes with the same idx but
 * lay in different nodes. Note that, MSR type boxes are bond to specific
 * cpu, manipulations of this type box should be called on wanted cpu.
 */
struct uncore_box {
	unsigned int		idx;
	unsigned int		nodeid;
	u64			hrtimer_duration;
	struct hrtimer		hrtimer;
	struct uncore_event	*event;
	struct uncore_box_type	*box_type;
	struct pci_dev		*pdev;
	struct list_head	next;
};

/**
 * struct uncore_box_ops
 * @show_box:
 * @init_box:
 * @enable_box:
 * @disable_box:
 * @enable_event:
 * @disable_event:
 * @write_counter:
 * @read_counter:
 * @write_filter:
 * @read_filter:
 *
 * Describe methods for manipulating a uncore PMU box. The methods are
 * microarchitecture specific. Some of them could be %NULL, e.g. read_filter.
 */
struct uncore_box_ops {                                              //这里是要改的
	void (*show_box)(struct uncore_box *box);
	void (*init_box)(struct uncore_box *box);
	void (*clear_box)(struct uncore_box *box);
	void (*enable_box)(struct uncore_box *box);
	void (*disable_box)(struct uncore_box *box);
	
	
	void (*enable_event)(struct uncore_box *box, struct uncore_event *event1, struct uncore_event *event2);
	void (*disable_event)(struct uncore_box *box, struct uncore_event *event1, struct uncore_event *event2);
	void (*write_counter)(struct uncore_box *box, u64 value1, u64 value2);
	void (*read_counter)(struct uncore_box *box, u64 *value1,u64 *value2);
	
	
	/*添加的函数部分*/     /*这里的绑定关系有点奇怪*/
/*	
	void (*enable_event_1)(struct uncore_box *box, struct uncore_event *event);
	void (*disable_event_1)(struct uncore_box *box, struct uncore_event *event);
	void (*write_counter_1)(struct uncore_box *box, u64 value);
	void (*read_counter_1)(struct uncore_box *box, u64 *value);
*/	
	
	
	void (*write_filter)(struct uncore_box *box, u64 value);
	void (*read_filter)(struct uncore_box *box, u64 *value);
};

/**
 * struct uncore_box_type
 * @name:		Name of this type box
 * @num_counters:	Counters this type box has
 * @num_boxes:		Boxes this type box has
 * @perf_ctr_bits:	Bit width of PMC
 
 
 * @perf_ctr:		PMC address
 * @perf_ctl:		EventSel address
 * @event_mask:		perf_ctl writable bits mask
 
 
 * @fixed_ctr_bits:	Bit width of fixed counter
 * @fixed_ctr:		Fixed counter address
 * @fixed_ctl:		Fixed EventSel address
 * @box_ctl:		Box-level Control address
 * @box_status:		Box-level Status address
 * @box_filter0:	Box-level Filter0 address
 * @box_filter1:	Box-level Filter1 address
 * @msr_offset:		MSR address offset of next box
 * @box_list:		List of all avaliable boxes of this type
 * @ops:		Box manipulation functions
 *
 * This struct describes a specific type of box. All box instances are linked
 * together. Each box type has its specific functions.
 */
struct uncore_box_type {
	const char	*name;
	unsigned int	num_counters;
	unsigned int	num_boxes;
	unsigned int	perf_ctr_bits;
	unsigned int	perf_ctr;
	unsigned int	perf_ctl;
	
	unsigned int	perf_ctr_1;          					//这里加了第二个ctr
	unsigned int 	perf_ctl_1;
	
	unsigned int	event_mask;
	unsigned int	fixed_ctr_bits;
	unsigned int	fixed_ctr;
	unsigned int	fixed_ctl;
	unsigned int	box_ctl;
	unsigned int	box_status;
	unsigned int	box_filter0;
	unsigned int	box_filter1;
	unsigned int	msr_offset;
	
	struct list_head box_list;
	const struct uncore_box_ops *ops;
};

/**
 * struct uncore_pmu
 * @name:		Name for uncore PMU
 * @n_node:		Number of online nodes
 * @pci_type:		PCI type boxes (%NULL if absent)
 * @msr_type:		MSR type boxes (can NOT be NULL)
 * @global_ctl:		MSR address of global control register (per socket)
 * @global_status:	MSR address of global status register (per socket)
 * @global_config:	MSR address of global config register (per socket)
 *
 * This structure is the TOP description about UNCORE_PMU. The main reason to
 * have such a global description structure is sometimes we need to manipulate
 * the global MSR registers, since the scope of these MSRs is per-socket. Almost
 * every microarchitecture has its global MSRs. Also, remember walking through
 * all online nodes when using this structure.
 */
struct uncore_pmu {
	const char		*name;
	unsigned int		n_node;
	struct uncore_box_type	**pci_type;
	struct uncore_box_type	**msr_type;
	unsigned int		global_ctl;
	unsigned int		global_status;
	unsigned int		global_config;
};

extern unsigned int uncore_socket_number;
extern struct uncore_box_type **uncore_msr_type;
extern struct uncore_box_type **uncore_pci_type;
extern struct pci_driver *uncore_pci_driver;
extern unsigned int uncore_pcibus_to_nodeid[256];
extern struct uncore_pmu uncore_pmu;

/*
 * PCI & MSR Type Box
 */

static inline u64 uncore_box_ctr_mask(struct uncore_box *box)
{
	return (1ULL << box->box_type->perf_ctr_bits) - 1;
}

/*
 * PCI Type Box
 */

static inline unsigned int uncore_pci_box_status(struct uncore_box *box)
{
	return box->box_type->box_status;
}

static inline unsigned int uncore_pci_box_ctl(struct uncore_box *box)
{
	return box->box_type->box_ctl;
}

static inline unsigned int uncore_pci_box_filter(struct uncore_box *box)
{
	return box->box_type->box_filter0;
}

static inline unsigned int uncore_pci_perf_ctl(struct uncore_box *box)        //这是一个的
{
	return box->box_type->perf_ctl;
}

static inline unsigned int uncore_pci_perf_ctl_1(struct uncore_box *box)     //这里也多了一个返回
{
	return box->box_type->perf_ctl_1;
}

static inline unsigned int uncore_pci_perf_ctr(struct uncore_box *box)        //这里改动了和下面
{
	return box->box_type->perf_ctr;
}

static inline unsigned int uncore_pci_perf_ctr_1(struct uncore_box *box)      //多一个返回
{
	return box->box_type->perf_ctr_1;
}

/*
 * MSR Type Box
 */

static inline unsigned int uncore_msr_box_offset(struct uncore_box *box)
{
	return box->idx * box->box_type->msr_offset;
}

static inline unsigned int uncore_msr_box_status(struct uncore_box *box)
{
	return box->box_type->box_status + uncore_msr_box_offset(box);
}

static inline unsigned int uncore_msr_box_ctl(struct uncore_box *box)                   
{
	return box->box_type->box_ctl + uncore_msr_box_offset(box);
}

static inline unsigned int uncore_msr_box_filter(struct uncore_box *box)
{
	return box->box_type->box_filter0 + uncore_msr_box_offset(box);
}

static inline unsigned int uncore_msr_perf_ctl(struct uncore_box *box)              //原本
{
	return box->box_type->perf_ctl + uncore_msr_box_offset(box);
}

static inline unsigned int uncore_msr_perf_ctr(struct uncore_box *box)
{
	return box->box_type->perf_ctr + uncore_msr_box_offset(box);
}


static inline unsigned int uncore_msr_perf_ctl_1(struct uncore_box *box)              //添加
{
	return box->box_type->perf_ctl_1 + uncore_msr_box_offset(box);
}

static inline unsigned int uncore_msr_perf_ctr_1(struct uncore_box *box)
{
	return box->box_type->perf_ctr_1 + uncore_msr_box_offset(box);
}




//这一部分是要重写的


/******************************************************************************
 * Generic Uncore PMU Box's APIs
 *****************************************************************************/

void uncore_clear_global_pmu(struct uncore_pmu *pmu);
void uncore_print_global_pmu(struct uncore_pmu *pmu);

int first_online_cpu_of_node(unsigned int node);
int uncore_call_function_on_node(unsigned int node, void (*func)(void *info), void *info, int wait);

void uncore_box_start_hrtimer(struct uncore_box *box);
void uncore_box_cancel_hrtimer(struct uncore_box *box);
void uncore_box_change_hrtimer(struct uncore_box *box, enum hrtimer_restart (*func)(struct hrtimer *hrtimer));
void uncore_box_change_duration(struct uncore_box *box, u64 new);


struct uncore_box *uncore_get_box(struct uncore_box_type *type, unsigned int idx, unsigned int nodeid);
struct uncore_box *uncore_get_first_box(struct uncore_box_type *type, unsigned int nodeid);

/**
 * uncore_box_bind_event
 * @box:	the box to bind													//这里得弄清楚 event到底和box之间的关系
 * @event:	the event to bind
 *
 * Bind a event to a box. Actually, it is uselee... This function just build a
 * link between box and event. When user invoke show_box, it can print some
 * nice information about current sampling or counting event.
 */
static inline void uncore_box_bind_event(struct uncore_box *box,            //这里可能要在结构体里面加一个事件2
					 struct uncore_event *event)
{
	if (box && event)
		box->event = event;
}

/**
 * uncore_show_box
 * @box:	the box to show
 *
 * Show control and counter status of the box
 */
static inline void uncore_show_box(struct uncore_box *box)
{
	if (box->box_type->ops->show_box)
		box->box_type->ops->show_box(box);
}

/**
 * uncore_init_box
 * @box:	the box to init
 *
 * Initialize a uncore box for a new event.
 * This method will clear the control and the counter registers.
 * Always call this if you want to begin a new event to count or sample.
 */
static inline void uncore_init_box(struct uncore_box *box)
{
	if (box->box_type->ops->init_box)
		box->box_type->ops->init_box(box);
}

/**
 * uncore_clear_box
 * @box:	the box to clear
 *
 * Clear control and counter registers of this box. Normally, it is the same
 * method with init_box. We have this function because the name 'clear' is more
 * proper when exit some monitoring session.
 */
static inline void uncore_clear_box(struct uncore_box *box)
{
	if (box->box_type->ops->clear_box)
		box->box_type->ops->clear_box(box);
}

/**
 * uncore_enable_box
 * @box:	the box to enable
 *
 * Enable the box to count or sample.
 * This method will enable counting at the box-level. Note that this method         
 * will *NOT* clear counters, use uncore_init_box to clear all registers.
 */
static inline void uncore_enable_box(struct uncore_box *box)
{
	if (box->box_type->ops->enable_box)
		box->box_type->ops->enable_box(box);
}

/**
 * uncore_disable_box
 * @box:	the box to disable
 *
 * Disable the box to count or sample.
 * This method will disbale counting at the box-level. Just freeze the counter.
 * You could re-enable counting or sampling by calling uncore_enable_box.
 */
static inline void uncore_disable_box(struct uncore_box *box)
{
	if (box->box_type->ops->disable_box)
		box->box_type->ops->disable_box(box);
}

/**
 * uncore_enable_event
 * @box:	the box to enable
 * @event:	the event to count or sample
 *																					 //这里是要改的enable-》event此处是打开计数器
 * Assign a specific event to box.															
 * This method will *NOT* start counting, call uncore_enable_box to start.
 */
static inline void uncore_enable_event(struct uncore_box *box,
				       struct uncore_event *event1,struct uncore_event *event2)
{
	if (box->box_type->ops->enable_event)
		box->box_type->ops->enable_event(box, event1, event2);
}

/**
 * uncore_disable_event
 * @box:	the box to disable
 * @event:	the event to disable
 *
 * Remove a specific event from box.
 * This method will *NOT* disable counting, call uncore_disable_box to stop.
 */
static inline void uncore_disable_event(struct uncore_box *box,
					struct uncore_event *event1,struct uncore_event *event2)
{
	if (box->box_type->ops->disable_event)
		box->box_type->ops->disable_event(box, event1, event2);
}

/**
 * uncore_write_counter
 * @box:	the box to write
 * @value:	the value to write
 *
 * Write to the counter of this box.
 * Most useful when sampling events.
 */
static inline void uncore_write_counter(struct uncore_box *box, u64 value1,u64 value2)
{
	if (box->box_type->ops->write_counter)
		box->box_type->ops->write_counter(box, value1, value2);
}

/**
 * uncore_read_counter
 * @box:	the box to read
 * @value:	place to hold value
 *
 * Read the counter of this box.
 * Lightweight show method, most useful when debugging.
 */
static inline void uncore_read_counter(struct uncore_box *box, u64 *value1,u64 *value2)
{
	if (box->box_type->ops->read_counter)
		box->box_type->ops->read_counter(box, value1,value2);
}



/**
 * uncore_write_filter
 * @box:	the box to write
 * @value:	the value to write
 *
 * Write the filter register of this box. Make sure you got the right filter
 * opcodes. Please consult the manual before you modify filters.
 */
static inline void uncore_write_filter(struct uncore_box *box, u64 value)
{
	if (box->box_type->ops->write_filter)
		box->box_type->ops->write_filter(box, value);
}

/**
 * uncore_read_filter
 * @box:	the box to read
 * @value:	place to hold value
 *
 * Read the filter register of this box.
 */
static inline void uncore_read_filter(struct uncore_box *box, u64 *value)
{
	if (box->box_type->ops->read_filter)
		box->box_type->ops->read_filter(box, value);
}

/******************************************************************************
 * /proc Part
 *****************************************************************************/

int uncore_proc_create(void);
void uncore_proc_remove(void);

/******************************************************************************
 * IMC Part
 *****************************************************************************/

/**
 * struct uncore_imc_ops
 * @set_threshold:
 * @enable_throttle:
 * @disable_throttle:
 *
 * CPU specific methods to manipulate a single IMC.
 */
struct uncore_imc_ops {
	int	(*set_threshold)(struct pci_dev *pdev, unsigned int threshold);
	int	(*enable_throttle)(struct pci_dev *pdev);
	void	(*disable_throttle)(struct pci_dev *pdev);
};

/**
 * struct uncore_imc
 * @nodeid:	Physcial node this imc on
 * @list:	Point to next imc device
 * @pdev:	the pci device instance
 * @ops:	Methods to manipulate IMC
 *
 * This structure describes the IMC device used in uncore. We have this
 * one mainly because we want to control the bandwith more convenient. 
 */
struct uncore_imc {
	unsigned int nodeid;
	struct list_head next;
	struct pci_dev *pdev;
	const struct uncore_imc_ops *ops;
};

extern const struct pci_device_id *uncore_imc_device_ids;
extern const struct uncore_imc_ops *uncore_imc_ops;
extern struct list_head uncore_imc_devices;

int uncore_imc_init(void);
void uncore_imc_exit(void);
void uncore_print_imc_devices(void);

int uncore_imc_set_threshold(unsigned int nodeid, unsigned int threshold);
int uncore_imc_enable_throttle(unsigned int nodeid);
void uncore_imc_disable_throttle(unsigned int nodeid);

int uncore_imc_set_threshold_all(unsigned int threshold);
int uncore_imc_enable_throttle_all(void);
void uncore_imc_disable_throttle_all(void);

/******************************************************************************
 * Micro-Architecture Specific Part
 *****************************************************************************/

/* Haswell-EP	*/
int hswep_cpu_init(void);
int hswep_pci_init(void);
int hswep_imc_init(void);
