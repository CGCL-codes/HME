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

#define pr_fmt(fmt) "UNCORE PROC: " fmt

#include "uncore_pmu.h"

#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static int bw_ratio = 1;
static DEFINE_MUTEX(uncore_proc_mutex);

static int pmu_proc_show(struct seq_file *file, void *v)
{
	seq_printf(file, "Bandwidth Throttling Ratio: 1/%d", bw_ratio);
	
	return 0;
}

static int uncore_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, pmu_proc_show, NULL);
}

/*
 * Control behaviour of the underlying module in a predefined manner
 */
static ssize_t uncore_proc_write(struct file *file, const char __user *buf,
				 size_t count,  loff_t *offs)
{
	char ctl[2];
	
	if (count !=2 || *offs)
		return -EINVAL;
	
	if (copy_from_user(ctl, buf, count))
		return -EFAULT;
	
	mutex_lock(&uncore_proc_mutex);
	switch (ctl[0]) {
		case '0':/* 1/1 Bandwidth */
			bw_ratio = 1;
			uncore_imc_set_threshold(0, 1);
			uncore_imc_set_threshold(1, 1);
			break;
		case '2':/* 1/2 Bandwidth */
			bw_ratio = 2;
			uncore_imc_set_threshold(0, 2);
			uncore_imc_set_threshold(1, 2);
			break;
		case '4':/* 1/4 Bandwidth */
			bw_ratio = 4;
			uncore_imc_set_threshold(0, 4);
			uncore_imc_set_threshold(1, 4);
			break;
		default:
			count = -EINVAL;
	}
	mutex_unlock(&uncore_proc_mutex);

	return count;
}

const struct file_operations uncore_proc_fops = {
	.open		= uncore_proc_open,
	.read		= seq_read,
	.write		= uncore_proc_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static bool is_proc_registed = false;

int uncore_proc_create(void)
{
	if (proc_create("uncore_pmu", 0644, NULL, &uncore_proc_fops)) {
		is_proc_registed = true;
		return 0;
	}

	return -ENOENT;
}

void uncore_proc_remove(void)
{
	if (is_proc_registed)
		remove_proc_entry("uncore_pmu", NULL);
}
