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
 * Basic information about intel uncore pmu:
 * Uncore performance monitors represent a per-socket resource that is not
 * meant to be affected by context switches and thread migration performed
 * by the OS, it is recommended that the monitoring software agent
 * establish a fixed affinity binding to prevent cross-talk of event count
 * from different uncore PMU.
 *
 * The programming interface of the counter registers and control regiters
 * fall into two address spaces:
 * $ Accessed by MSR are PMON registers within the CBo, SBo, PCU, U-Box.
 * $ Accessed by PCI device configuration space are PMON registers within
 *   the HA, IMC, Intel QPI, R2PCIe and R3QPI units.
 */

#define pr_fmt(fmt) "UNCORE PMU: " fmt

#include "uncore_pmu.h"
#include "emulate_nvm.h"

#include <asm/setup.h>

#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hrtimer.h>
#include <linux/cpumask.h>

/*
 * This is the top description of whole system uncore pmu resources.
 */
struct uncore_pmu uncore_pmu;

unsigned int uncore_pcibus_to_nodeid[256] = { [0 ... 255] = -1, };

struct uncore_box_type *dummy_xxx_type[] = { NULL, };
struct uncore_box_type **uncore_msr_type = dummy_xxx_type;
struct uncore_box_type **uncore_pci_type = dummy_xxx_type;

/*
 * Since kernel has a uncore PMU module which has claimed all the PCI boxes
 * at kernel startup, so this uncore_pci_probe method will never get called.
 * I leave pci driver and these two methods here for future usage.
 */
struct pci_driver *uncore_pci_driver;
static void __always_unused uncore_pci_remove(struct pci_dev *dev) {}
static int __always_unused uncore_pci_probe(struct pci_dev *dev,
					    const struct pci_device_id *id)
{return -EIO;}

/**
 * first_online_cpu_of_node
 * @node:	The node to search
 * @Return:	Positive-value on success
 *
 * Find the first online cpu within a physical node. Note that the cpumask of
 * node only include online cpus, so there is no need to use cpumask_first_and
 */
int first_online_cpu_of_node(unsigned int node)
{
	int cpu = -1;
	const struct cpumask *mask;

	mask = cpumask_of_node(node);
	cpu = cpumask_first(mask);

	if (cpu > nr_cpu_ids)
		cpu = -1;
	
	return cpu;
}

/**
 * uncore_call_function_on_node
 * @node:	The node to execute function
 * @func:	The function to execute
 * @info:	The info passed to function
 * @wait:	If true, wait until function finished on other cpus
 *
 * Call a simple and fast function on a node. It can run on any online cpus
 * within the node. This functions is a simple wrapper.
 */
int uncore_call_function_on_node(unsigned int node,
				void (*func)(void *info), void *info, int wait)
{
	int cpu, err;

	if (node > nr_node_ids || !func)
		return -EINVAL;

	cpu = first_online_cpu_of_node(node);
	if (cpu < 0)
		return -ENXIO;

	err = smp_call_function_single(cpu, func, info, wait);

	return err;
}

/*
 * This is the default hrtimer function.
 */
static enum hrtimer_restart uncore_box_hrtimer_def(struct hrtimer *hrtimer)
{
	struct uncore_box *box;

	box = container_of(hrtimer, struct uncore_box, hrtimer);

	hrtimer_forward_now(hrtimer, ns_to_ktime(box->hrtimer_duration));

	return HRTIMER_RESTART;
}

/**
 * uncore_box_init_hrtimer
 * @uncore_box:		The box who asked to init hrtimer
 * @func:		Restart function when polled
 *
 * According to kernel developers:
 * The overflow interrupt is unavailable for SandyBridge-EP, is broken for
 * SandyBridge. So we use hrtimer to periodically poll the counter to avoid
 * overflow. Different boxes can have their own funcs to handle this.
 */
static void uncore_box_init_hrtimer(struct uncore_box *box,
			enum hrtimer_restart (*func)(struct hrtimer *hrtimer))
{
	hrtimer_init(&box->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	box->hrtimer.function = func;
}

void uncore_box_start_hrtimer(struct uncore_box *box)
{
	/* Time is relative to now, and is bound to cpu */
	hrtimer_start(&box->hrtimer, ns_to_ktime(box->hrtimer_duration),
		HRTIMER_MODE_REL);
}

void uncore_box_cancel_hrtimer(struct uncore_box *box)
{
	hrtimer_cancel(&box->hrtimer);
}

void uncore_box_change_hrtimer(struct uncore_box *box,
			enum hrtimer_restart (*func)(struct hrtimer *hrtimer))
{
	box->hrtimer.function = func;
}

void uncore_box_change_duration(struct uncore_box *box, u64 new)
{
	box->hrtimer_duration = new;
}

/**
 * uncore_get_box
 * @type:	pointer to box_type
 * @idx:	idx of the box in this box_type
 * @nodeid:	which NUMA node to get this box
 * Return:	%NULL on failure
 *
 * Get a uncore PMU box to perform tasks. Note that each box of its type has
 * its dedicated idx number, and belongs to a specific NUMA node. Therefore, to
 * get a PMU box you have to offer all these three parameters. Besides, you can
 * see the idx information after print_boxes.
 */
struct uncore_box *uncore_get_box(struct uncore_box_type *type,
				  unsigned int idx, unsigned int nodeid)
{
	struct uncore_box *box;

	if (!type || idx < 0 || nodeid > UNCORE_MAX_SOCKET)
		return NULL;

	list_for_each_entry(box, &type->box_list, next) {
		if (box->idx == idx && box->nodeid == nodeid)
			return box;
	}

	return NULL;
}

/**
 * uncore_get_first_box
 * @type:	pointer to box_type
 * @nodeid:	which NUMA node to get this box
 * Return:	%NULL on failure
 *
 * Get the first box in the box_type list of @nodeid node. We have this
 * function because some box types only have one avaliable box within a node.
 * It is more convenient to get box without an idx. (I know...)
 */
struct uncore_box *uncore_get_first_box(struct uncore_box_type *type,
					unsigned int nodeid)
{
	struct uncore_box *box;

	if (!type || nodeid > UNCORE_MAX_SOCKET)
		return NULL;

	list_for_each_entry(box, &type->box_list, next) {
		if (box->nodeid == nodeid)
			return box;
	}

	return NULL;
}

static void __uncore_clear_global_pmu(void *info)
{
	unsigned int status;
	struct uncore_pmu *pmu;

	pmu = (struct uncore_pmu *)info;

	if (pmu->global_ctl) {
		wrmsrl(pmu->global_ctl, 0);
	}

	if (pmu->global_status) {
		/* RW1C */
		rdmsrl(pmu->global_status, status);
		wrmsrl(pmu->global_status, status);
	}
}

/**
 * uncore_clear_global_pmu
 * @pmu:	The uncore_pmu to clear/reset
 *
 * This function will c
 */
void uncore_clear_global_pmu(struct uncore_pmu *pmu)
{
	unsigned int node;

	if (!pmu)
		return;

	for_each_online_node(node) {
		uncore_call_function_on_node(node, __uncore_clear_global_pmu,
				(void *)pmu, 1);
	}
}

static void __uncore_print_global_pmu(void *info)
{
	unsigned int config;
	struct uncore_pmu *pmu;

	pmu = (struct uncore_pmu *)info;
	
	/* Careful, invalid address would cause #GP */
	if (pmu->global_ctl) {
		rdmsrl(pmu->global_ctl, config);
		pr_info("...... PMU Global Control: 0x%x", config);
	}

	if (pmu->global_status) {
		rdmsrl(pmu->global_status, config);
		pr_info("...... PMU Global Status:  0x%x", config);
	}

	if (pmu->global_config) {
		rdmsrl(pmu->global_config, config);
		pr_info("...... PMU Global Config:  0x%x", config);
	}
}

/**
 * uncore_print_global_pmu
 * @pmu:	the uncore_pmu in question
 *
 * Display the contents of the three global registers. Normally, every
 * microarchitecture has global registers including: CONTROL/STATUS/CONFIG.
 * This function goes through all online nodes.
 */
void uncore_print_global_pmu(struct uncore_pmu *pmu)
{
	unsigned int node;

	if (!pmu)
		return;

	pr_info("\033[34m------------------------ Global PMU ----------------------\033[0m");
	pr_info("Description: %s", pmu->name);

	for_each_online_node(node) {
		pr_info("On Node %d:", node);
		uncore_call_function_on_node(node, __uncore_print_global_pmu,
				(void *)pmu, 1);
	}
}

/**
 * uncore_print_node_info
 *
 * Just like command lscpu, this function print the node and cpu mapping
 * information. PMU is hard-to-code mainly because it has to be read/write on
 * the *right* cpu or node. Keep that in mind when coding pmu programs.
 */
static void uncore_print_node_info(void)
{
	int node, cpu;
	const struct cpumask *mask;

	pr_info("\033[34m------------------------ Node Info ----------------------\033[0m");
	for_each_online_node(node) {
		pr_info("Online Node %d", node);
		mask = cpumask_of_node(node);
		for_each_cpu_and(cpu, mask, cpu_online_mask) {
			pr_info("... Online CPU %d", cpu);
		}
	}
}

/**
 * uncore_print_pci_boxes
 * 
 * Print information about all avaliable PCI type boxes. Read this to make sure
 * your CPU has the capacity you need before sampling or counting uncore PMU.
 */
static void uncore_print_pci_boxes(void)
{
	struct uncore_box_type *type;
	struct uncore_box *box;
	int i;

	pr_info("\033[34m------------------------ PCI Type Boxes ----------------------\033[0m");
	for (i = 0; uncore_pci_type[i]; i++) {
		type = uncore_pci_type[i];
		pr_info("PCI Type: %s Boxes: %d",
			type->name,
			list_empty(&type->box_list)? 0: type->num_boxes);

		list_for_each_entry(box, &type->box_list, next) {
			pr_info("......Box%d, in Node%d, %x:%x:%x, %d:%d:%d, Kref = %d",
			box->idx,
			box->nodeid,
			box->pdev->bus->number,
			box->pdev->vendor,
			box->pdev->device,
			box->pdev->bus->number,
			(box->pdev->devfn >> 3) & 0x1f,
			(box->pdev->devfn) & 0x7,
			box->pdev->dev.kobj.kref.refcount.refs.counter);
		}
		pr_info("\n");
	}
}

/**
 * uncore_print_pci_mapping
 *
 * Print the mapping information between PCI bus number and numa node id.
 * It seems kernel has a internal mapping table, too... lol
 */
static void uncore_print_pci_mapping(void)
{
	int bus;

	pr_info("\033[34m------------------------ PCI Bus No. Mapping ----------------------\033[0m");
	for (bus = 0; bus < 256; bus++) {
		if (uncore_pcibus_to_nodeid[bus] != -1) {
			pr_info("......BUS %d (0x%x) <---> NODE %d",
				bus, bus, uncore_pcibus_to_nodeid[bus]);
		}
	}
}

/**
 * uncore_print_msr_boxes
 * 
 * Print information about all avaliable MSR type boxes. Read this to make sure
 * your CPU has the capacity you need before sampling or counting uncore PMU.
 */
static void uncore_print_msr_boxes(void)
{
	struct uncore_box_type *type;
	struct uncore_box *box;
	int i;
	
	pr_info("\033[34m------------------------ MSR Type Boxes ----------------------\033[0m");
	for (i = 0; uncore_msr_type[i]; i++) {
		type = uncore_msr_type[i];
		pr_info("MSR Type: %s Boxes: %d", type->name,
			list_empty(&type->box_list)?0:type->num_boxes);

		list_for_each_entry(box, &type->box_list, next) {
			pr_info("......Box%d, in Node%d",
			box->idx,
			box->nodeid);
		}
		pr_info("\n");
	}
}

/**
 * uncore_types_init
 * @types:	box_type to init
 * Return:	Non-zero on failure
 *
 * Init the array of **uncore_box_type. Specially, the list_head.
 * This function should be called *after* CPU-specific init function.
 */
static int uncore_types_init(struct uncore_box_type **types)
{
	int i;

	for (i = 0; types[i]; i++) {
		INIT_LIST_HEAD(&types[i]->box_list);
	}

	return 0;
}

/**
 * uncore_pci_new_box
 * @pdev:	the pci device of this box
 * @id:		the device id of this box
 * Return:	Non-zero on failure
 *
 * Malloc a new box of PCI type, initilize all the fields. And then insert it
 * into the tail of box_list of its uncore_box_type.
 */
static int __must_check uncore_pci_new_box(struct pci_dev *pdev,
					   const struct pci_device_id *id)
{
	struct uncore_box_type *type;
	struct uncore_box *box, *last;

	type = uncore_pci_type[UNCORE_PCI_DEV_TYPE(id->driver_data)];
	if (!type)
		return -EFAULT;

	box = kzalloc(sizeof(struct uncore_box), GFP_KERNEL);
	if (!box)
		return -ENOMEM;
	
	if (list_empty(&type->box_list)) {
		box->idx = 0;
		type->num_boxes = 1;
	} else {
		last = list_last_entry(&type->box_list, struct uncore_box, next);
		box->idx = last->idx + 1;
		type->num_boxes++;
	}
	
	uncore_box_init_hrtimer(box, uncore_box_hrtimer_def);
	box->hrtimer_duration = UNCORE_PMU_HRTIMER_INTERVAL;
	box->nodeid = uncore_pcibus_to_nodeid[pdev->bus->number];
	box->box_type = type;
	box->pdev = pdev;
	list_add_tail(&box->next, &type->box_list);
	
	return 0;
}

/* Free all PCI type boxes */
static void uncore_pci_exit(void)
{
	struct uncore_box_type *type;
	struct uncore_box *box;
	struct list_head *head;
	int i;

	for (i = 0; uncore_pci_type[i]; i++) {
		type = uncore_pci_type[i];
		head = &type->box_list;
		while (!list_empty(head)) {
			box = list_first_entry(head, struct uncore_box, next);
			list_del(&box->next);
			/* Since we have get_device manually */
			pci_dev_put(box->pdev);
			kfree(box);
		}
	}
}

/* Malloc all PCI type boxes */
static int __must_check uncore_pci_init(void)
{
	const struct pci_device_id *ids;
	struct pci_dev *pdev;
	int ret;

	ret = -ENXIO;
	switch (boot_cpu_data.x86_model) {
		case 45: /* Sandy Bridge-EP*/
			break;
		case 62: /* Ivy Bridge-EP */
			break;
		case 63: /* Haswell-EP */
			ret = hswep_pci_init();
			break;
	};

	if (ret)
		return ret;

	ret = uncore_types_init(uncore_pci_type);
	if (ret)
		return ret;

	ids = uncore_pci_driver->id_table;
	if (!ids)
		return -EFAULT;

	for (; ids->vendor || ids->device; ids++) {
		/* Iterate over all PCI buses */
		pdev = NULL;
		while (1) {
			pdev = pci_get_device(ids->vendor, ids->device, pdev);
			if (!pdev)
				break;
			
			/* BIG FAT NOTE
			 * pci_get_device will call pci_dev_put to put pdev.
			 * As a consequence, every pdev actually does *NOT*
			 * increase its kref at all! So we have to manually
			 * increase pdev's kref counter in case these pdevs
			 * are released! (we will put pdev when pci_exit)
			 */
			get_device(&pdev->dev);

			ret = uncore_pci_new_box(pdev, ids);
			if (ret)
				goto error;
		}
	}

	return 0;

error:
	uncore_pci_exit();
	return ret;
}

/**
 * uncore_msr_new_box
 * @type:	the MSR box_type
 * @idx:	the idx of the new box
 * Return:	Non-zero on failure
 *
 * Malloc a new box of MSR type, and then insert it into the tail
 * of box_list of its uncore_box_type.
 */
static int __must_check uncore_msr_new_box(struct uncore_box_type *type,
					   unsigned int idx)
{
	struct uncore_box *box;

	if (!type)
		return -EINVAL;

	box = kzalloc(sizeof(struct uncore_box), GFP_KERNEL);
	if (!box)
		return -ENOMEM;

	uncore_box_init_hrtimer(box, uncore_box_hrtimer_def);
	box->hrtimer_duration = UNCORE_PMU_HRTIMER_INTERVAL;
	box->idx = idx;
	box->nodeid = 0;	/* XXX */
	box->box_type = type;
	list_add_tail(&box->next, &type->box_list);

	return 0;
}

/* Free MSR type boxes */
static void uncore_cpu_exit(void)
{
	struct uncore_box_type *type;
	struct uncore_box *box;
	struct list_head *head;
	int i;

	for (i = 0; uncore_msr_type[i]; i++) {
		type = uncore_msr_type[i];
		head = &type->box_list;
		while (!list_empty(head)) {
			box = list_first_entry(head, struct uncore_box, next);
			list_del(&box->next);
			kfree(box);
		}
	}
}

/* Malloc MSR type boxes */
static int __must_check uncore_cpu_init(void)
{
	struct uncore_box_type *type;
	unsigned int idx;
	int n, ret;

	ret = -ENXIO;
	switch (boot_cpu_data.x86_model) {
		case 45: /* Sandy Bridge-EP*/
			break;
		case 62: /* Ivy Bridge-EP */
			break;
		case 63: /* Haswell-EP */
			ret = hswep_cpu_init();
			break;
	};

	if (ret)
		return ret;

	ret = uncore_types_init(uncore_msr_type);
	if (ret)
		return ret;
	
	for (n = 0; uncore_msr_type[n]; n++) {
		type = uncore_msr_type[n];
		for (idx = 0; idx < type->num_boxes; idx++) {
			ret = uncore_msr_new_box(type, idx);
			if (ret)
				goto error;
		}
	}

	return 0;

error:
	uncore_cpu_exit();
	return ret;
}

static int uncore_init(void)
{
	int ret;

	pr_info("\033[34mINIT ON CPU %2d (NODE %2d)\033[0m",
		smp_processor_id(), numa_node_id());

	ret = uncore_pci_init();
	if (ret)
		goto pcierr;

	ret = uncore_cpu_init();
	if (ret)
		goto cpuerr;

	ret = uncore_imc_init();
	if (ret)
		goto out;

	ret = uncore_proc_create();
	if (ret)
		goto out;

	/*
	 * Pay attention to these messages
	 * Check if everything goes as expected
	 */
	uncore_clear_global_pmu(&uncore_pmu);
	uncore_print_global_pmu(&uncore_pmu);
	uncore_print_node_info();
	uncore_print_msr_boxes();
	uncore_print_pci_boxes();
	uncore_print_pci_mapping();
	uncore_print_imc_devices();
	
	/*
	 * Emulation is just a client of PMU :)
	 */
	start_emulate_nvm();

	return 0;

out:
	uncore_imc_exit();
cpuerr:
	uncore_cpu_exit();
pcierr:
	uncore_pci_exit();
	return ret;
}

static void uncore_exit(void)
{
	/*
	 * Game over, back to DRAM
	 */
	finish_emulate_nvm();
	
	uncore_clear_global_pmu(&uncore_pmu);
	uncore_proc_remove();
	uncore_imc_exit();
	uncore_cpu_exit();
	uncore_pci_exit();
	
	pr_info("EXIT ON CPU %2d (NODE %2d)",
		smp_processor_id(), numa_node_id());
}

module_init(uncore_init);
module_exit(uncore_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("shanyizhou@ict.ac.cn");
