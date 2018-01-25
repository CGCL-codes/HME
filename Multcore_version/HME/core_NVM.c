#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/printk.h> 
#include <linux/kobject.h> 
#include <linux/sysfs.h> 
#include <linux/string.h> 
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AriesLiu");
MODULE_DESCRIPTION("This is a NVM module!\n");

//static char buf1[10];

static struct kobject *coreNVM_kobject;
static int nvm;

int my_atoi(const char *str)
{
    int value = 0;
    int flag = 1; 

    while (*str == ' ')  //skip space
    {
        str++;
    }

    if (*str == '-')  //negative
    {
        flag = 0;
        str++;
    }
    else if (*str == '+') //positive
    {
        flag = 1;
        str++;
    }
    else if (*str >= '9' || *str <= '0')
    {
        return 0;
    }
    while (*str != '\0' && *str <= '9' && *str >= '0')
    {        
           value = value * 10 + *str - '0'; 
           str++;
    }  
    if (flag == 0) 
    {
	 value = -value;
    }  
    return value;
}
static void send_delay(void *info)
{
	int delay = *(int *)info;
	mdelay(delay);
}

static ssize_t coreNVM_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	//printk(KERN_EMERG "reading data\n");
	return sprintf(buf, "%d", nvm);
}

static ssize_t coreNVM_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	//printk(KERN_EMERG "%s", buf);
	int cpu_id,time;
	int length = strlen(buf);
	char *cpu_C=(char *)kmalloc(sizeof(char)*1,GFP_ATOMIC);
	char *time_C=(char *)kmalloc(sizeof(char)*(length-2),GFP_ATOMIC);
	strncpy(cpu_C,buf,1);
	strncpy(time_C,buf+1,length-1);
	cpu_id = my_atoi(cpu_C);
	time = my_atoi(time_C);
	//printk(KERN_EMERG "cpu_C - %d  time_C - %d",cpu_id,time);
	smp_call_function_single(cpu_id, send_delay, &time, 1);
	kfree(cpu_C);
	kfree(time_C);
	sscanf(buf, "%d", &nvm);
	return count;
}

static struct kobj_attribute coreNVM_attribute = __ATTR(nvm, 0660, coreNVM_show, coreNVM_store);

static int __init core_NVM_init(void)
{
	int error = 0;
	//printk(KERN_EMERG "NVMain init\n");
	pr_debug("Module initialized successfully \n");

	coreNVM_kobject = kobject_create_and_add("kobject_NVM", kernel_kobj);
	if (!coreNVM_kobject)
		return -ENOMEM;

	error = sysfs_create_file(coreNVM_kobject, &coreNVM_attribute.attr);
	if (error) {
		pr_debug("failed to create the nvm file in /sys/kernel/kobject_example \n");
	}
	
	return 0;
}

static void __exit core_NVM_exit(void)
{
	//printk(KERN_EMERG "NVM, exit\n");
	kobject_put(coreNVM_kobject);
}

module_init(core_NVM_init);
module_exit(core_NVM_exit);
