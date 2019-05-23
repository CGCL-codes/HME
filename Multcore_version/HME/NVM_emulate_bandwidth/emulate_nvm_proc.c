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

#include "emulate_nvm.h"

#include <asm/uaccess.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

static int emulate_nvm_proc_show(struct seq_file *m, void *v)
{	
	return 0;
}

static int emulate_nvm_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, emulate_nvm_proc_show, NULL);
}

static ssize_t emulate_nvm_proc_write(struct file *file, const char __user *buf,
				   size_t count, loff_t *offs)
{
	char ctl[2];
	
	if (count !=2 || *offs)
		return -EINVAL;
	
	if (copy_from_user(ctl, buf, count))
		return -EFAULT;
	
	switch (ctl[0]) {
		/*TODO modify parameters */
		default:
			count = -EINVAL;
	}

	return count;
}

const struct file_operations emulate_nvm_proc_fops = {
	.open		= emulate_nvm_proc_open,
	.read		= seq_read,
	.write		= emulate_nvm_proc_write,
	.llseek		= seq_lseek,
	.release	= single_release
};

static bool is_proc_registed = false;

int __must_check emulate_nvm_proc_create(void)
{
	if (proc_create("emulate_nvm", 0444, NULL, &emulate_nvm_proc_fops)) {
		is_proc_registed = true;
		return 0;
	}

	return -ENOENT;
}

void emulate_nvm_proc_remove(void)
{
	if (is_proc_registed)
		remove_proc_entry("emulate_nvm", NULL);
}
