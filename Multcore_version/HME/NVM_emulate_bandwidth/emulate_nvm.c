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

#define pr_fmt(fmt) fmt

#include "uncore_pmu.h"
#include "emulate_nvm.h"

#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>

unsigned int polling_cpu;
unsigned int polling_node;
unsigned int emulate_nvm_cpu;
unsigned int emulate_nvm_node;

static bool emulation_started = false;

static int start_emulate_bandwidth(void)
{
	/* default to full bandwidth */
	uncore_imc_set_threshold_all(1);

	/* enable throttling at all nodes */
	uncore_imc_enable_throttle_all();

	return 0;
}

static void finish_emulate_bandwidth(void)
{
	/* disable throttling at all nodes */
	uncore_imc_disable_throttle_all();
}

void show_emulate_parameter(void)
{
	pr_info("------------------------ Emulation Parameters ----------------------");

	pr_info("Polling CPU:  CPU%2d (Node %2d)", polling_cpu, polling_node);
	pr_info("Emulated CPU: CPU%2d (Node %2d)", emulate_nvm_cpu, emulate_nvm_node);
	
	pr_info("------------------------ Emulation Parameters ----------------------");
}

static int prepare_platform_configuration(void)
{
	int cpu;
	const struct cpumask *mask;
	
	cpu = smp_processor_id();
	if (cpu != polling_cpu) {
		printk(KERN_CONT "ERROR: current CPU:%2d is not polling CPU:%2d... ",
			cpu, polling_cpu);
		return -1;
	}

	/*
 	 * In hybrid-memory configuration model, CPUs except the
	 * emulating one must be offlined. We have to do this because
	 * the 'ha_requests_remote_reads' event can _not_ distinguish
	 * requests from different cpus. To gain a 'best' emulation model,
	 * only the emulating cpu can alive!
 	 */
	mask = cpumask_of_node(polling_node);
	for_each_cpu(cpu, mask) {
		if (cpu != polling_cpu)
			cpu_down(cpu);
	}
	return 0;
}

static void restore_platform_configuration(void)
{
	int cpu;
	const struct cpumask *mask;

	mask = cpumask_of_node(polling_node);
	for_each_cpu_not(cpu, mask) {
		if (cpu != polling_cpu)
			cpu_up(cpu);
	}
}

#define pr_fail		printk(KERN_CONT "\033[31m fail \033[0m")
#define pr_okay		printk(KERN_CONT "\033[32m okay \033[0m")

#define PR_RESULT()	(ret)? pr_fail: pr_okay

void start_emulate_nvm(void)
{
	int ret;

	/*
	 * Polling CPU is the one always polling uncore pmu
	 * and sending IPI delay function to emulate_nvm_cpu.
	 *
	 * Emulate NVM CPU is the one used to emulate NVM,
	 * also the receiver of IPI sent from polling cpu.
	 */
	polling_cpu = 12;
	emulate_nvm_cpu = 0;

	polling_node = cpu_to_node(polling_cpu);
	emulate_nvm_node = cpu_to_node(emulate_nvm_cpu);

	show_emulate_parameter();

	pr_info("creating /proc/emulate_nvm... ");
	ret = emulate_nvm_proc_create();
	PR_RESULT();
	if (ret)
		return;

	pr_info("preparing platform... ");
	ret = prepare_platform_configuration();
	PR_RESULT();
	if (ret)
		goto out;
	
	pr_info("emulating bandwidth... ");
	ret = start_emulate_bandwidth();
	PR_RESULT();
	if (ret)
		goto out1;
	
	emulation_started = true;
	return;

out1:
	restore_platform_configuration();
out:
	emulate_nvm_proc_remove();
}

void finish_emulate_nvm(void)
{
	if (emulation_started) {
		emulate_nvm_proc_remove();
		restore_platform_configuration();
		finish_emulate_bandwidth();
		pr_info("finish emulating nvm... ");
		emulation_started = false;
	}
}
