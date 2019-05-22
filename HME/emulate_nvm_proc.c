/*
 *	Copyright (C) 2016-2017 Zhuohui Duan <zhduan@hust.edu.cn>
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

//#include <string.h>
//#include <stdlib.h>

#include <linux/list.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>


extern u64 read_latency_delta_ns;
extern u64 write_latency_delta_ns;
extern u64 hrtimer_jiffies;

u64 DRAM_Read_latency;
u64 DRAM_Write_latency;
u64 NVM_Read_latency;
u64 NVM_Write_latency;

u64 epoch_duration_us;

u64 NVM_write_w;
u64 NVM_read_w;
u64 DRAM_read_w;
u64 DRAM_write_w;

u64 read_counts=0;
u64 write_counts=0;

u64 local_read_counts=0;
u64 local_write_counts=0;

static DEFINE_MUTEX(emulate_proc_mutex);

#define STRINGLEN 1024
char global_buffer[STRINGLEN];

int StrToInt(const char* str);
long long StrToIntCore(const char* digit, bool minus);
char *strtok(char *str, const char *delim);
//float atof(char s[]);

static int emulate_nvm_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "this moment, NVM write counts=%llu, delay_ns=%llu, NVM read counts=%llu, delay_ns=%llu\n",
			write_counts, write_counts*write_latency_delta_ns,read_counts, read_counts*read_latency_delta_ns);

    seq_printf(m, "Dram read counts = %llu, Dram write counts = %llu\n", local_read_counts, local_write_counts);
    seq_printf(m, "total jiffies = %llu\n", hrtimer_jiffies);

    seq_printf(m, "DRAM read latency : %llu ns\n", DRAM_Read_latency);
    seq_printf(m, "DRAM write latency : %llu ns\n",DRAM_Write_latency);
    seq_printf(m, "NVM read latency : %llu ns\n",NVM_Read_latency);
    seq_printf(m, "NVM write latency : %llu ns\n",NVM_Write_latency);
    seq_printf(m, "Epoch duration time : %llu us\n",epoch_duration_us);
    seq_printf(m, "NVM read consumption: %llu J\n",NVM_read_w*read_counts);
    seq_printf(m, "NVM write consumption: %llu J\n",NVM_write_w*write_counts);
    seq_printf(m, "DRAM read consumption: %llu J\n",DRAM_read_w*local_read_counts);
    seq_printf(m, "DRAM write consumption: %llu J\n",DRAM_write_w*local_write_counts);
    //seq_printf(m, "%d,%d\n",read_latency_delta_ns, write_latency_delta_ns);

	return 0;
}

static int emulate_nvm_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, emulate_nvm_proc_show, NULL);
}

static ssize_t emulate_nvm_proc_write(struct file *file, const char __user *buf,
				   size_t count, loff_t *offs)
{
	int len = count;
	int temp[30];
    int i=0;
    char *result = NULL;
    char delims[] = ",";

	if (copy_from_user(global_buffer, buf, count))
		return -EFAULT;

    mutex_lock(&emulate_proc_mutex);
    global_buffer[len] = '\0';
    result = strtok(global_buffer,delims);
    while(result != NULL)
    {
        temp[i++] = StrToInt(result);
        result = strtok(NULL,delims);
    }

    DRAM_Read_latency = temp[0];
    DRAM_Write_latency = temp[1];
    NVM_Read_latency = temp[2];
    NVM_Write_latency = temp[3];
    epoch_duration_us = temp[4];
    NVM_read_w = temp[5];
    NVM_write_w = temp[6];
    DRAM_read_w = temp[7];
    DRAM_write_w = temp[8];

    pr_info("SET EMULATE_CONFIG");
    emulate_set_config(NVM_Read_latency-DRAM_Read_latency,NVM_Write_latency-DRAM_Write_latency,epoch_duration_us*1000000);
    mutex_unlock(&emulate_proc_mutex);
    pr_info("leave function");
    show_emulate_parameter();
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

int emulate_nvm_proc_create(void)
{
	if (proc_create("emulate_nvm", 0666, NULL, &emulate_nvm_proc_fops)) {
		is_proc_registed = true;
		return 0;
	}

	return -ENOENT;
}

void emulate_nvm_proc_remove(void)
{
    pr_info("emulate_nvm_proc_remove is call\n");
	if (is_proc_registed){
	    pr_info("is_proc_registed is true call remov_proc_entry\n");
        remove_proc_entry("emulate_nvm", NULL);  //zheli
        pr_info("remove_proc_entry is over\n");
    }
}


enum Status {kValid=0, kInvalid};
int g_nStatus = kValid;

long long StrToIntCore(const char* digit, bool minus) {
    int num = 0;
    while (*digit != '\0') {
        if (*digit >= '0' && *digit <= '9') {
            int flag; 
            flag = minus ? -1 : 1;
            num = num*10 + flag*(*digit-'0');
            if ((!minus && num>0x7FFFFFFF) || (minus && num <(signed int)0x80000000)) {
                num = 0;
                break;
            }
            digit++;
        } else {
            num = 0;
            break;
        }
    }
    if (*digit == '\0') {
        g_nStatus = kValid;
    }
    return num;
}

int StrToInt(const char* str) {
    int num = 0; 
    g_nStatus = kInvalid;
    if (str != NULL && *str != '\0') {
        bool minus = false;
        if (*str == '+')
            str++;
        else if (*str == '-') {
                str++;
                minus = true;
            }
        if (*str != '\0')
            num = StrToIntCore(str, minus);
    }
    return (int)num;
}

char *strtok(char *str, const char *delim)
{
    static char *src= NULL;
    const char *indelim=delim;
    int flag=1,index=0;                                
    char *temp= NULL;
    if(str==NULL)
    {
        str=src;
    }
    for(;*str;str++)
    {
        delim=indelim;
        for(;*delim;delim++)
        {
            if(*str==*delim)
            {
                *str = '\0';
                index=1; 
                break;
            }
        }
        if(*str!='\0'&&flag==1)
        {
            temp=str;
            flag=0;  
        }
        if(*str!='\0'&&flag==0&&index==1)
        {
            src=str;
            return temp;
        }
    }
    src=str;                              
    return temp;
}
/*
int isspace(char c)
{
    if(c =='\t'|| c =='\n'|| c ==' ')
        return 1;
    else
        return 0;
}

#define isdigit(c) ((c) >= '0' && (c) <= '9')

float atof(char s[]){
    float val, power;
    int i, sign;

    for(i = 0; isspace(s[i]); i++){

    }
    sign = (s[i]== '-')? -1 : 1;
    for(val = 0.0; isdigit(s[i]);i++){
        val = 10.0*val + s[i] - '0';
    }
    if (s[i] == '.'){
        i++;
    }
    for(power = 1.0; isdigit(s[i]);i++){
        val = 10.0*val+s[i]-'0';
        power=power*10.0;
    }
    return sign*val/power;
}
*/
