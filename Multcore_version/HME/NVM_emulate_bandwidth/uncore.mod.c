#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x28950ef1, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x7cb1ae69, __VMLINUX_SYMBOL_STR(cpu_down) },
	{ 0x98ab5c8d, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x7f59e59, __VMLINUX_SYMBOL_STR(pci_write_config_dword) },
	{ 0xfd96706c, __VMLINUX_SYMBOL_STR(node_to_cpumask_map) },
	{ 0x8bd590db, __VMLINUX_SYMBOL_STR(pci_write_config_word) },
	{ 0x16a5a12f, __VMLINUX_SYMBOL_STR(single_open) },
	{ 0xe7f3608a, __VMLINUX_SYMBOL_STR(hrtimer_forward) },
	{ 0x930484aa, __VMLINUX_SYMBOL_STR(cpu_online_mask) },
	{ 0x2296f507, __VMLINUX_SYMBOL_STR(single_release) },
	{ 0x45449b56, __VMLINUX_SYMBOL_STR(boot_cpu_data) },
	{ 0xfe26fc7c, __VMLINUX_SYMBOL_STR(nr_node_ids) },
	{ 0x94313d7, __VMLINUX_SYMBOL_STR(hrtimer_cancel) },
	{ 0xc0a3d105, __VMLINUX_SYMBOL_STR(find_next_bit) },
	{ 0x74df1d4, __VMLINUX_SYMBOL_STR(seq_printf) },
	{ 0xa16aae11, __VMLINUX_SYMBOL_STR(remove_proc_entry) },
	{ 0x4ed12f73, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0x54efb5d6, __VMLINUX_SYMBOL_STR(cpu_number) },
	{ 0x9c3df9b4, __VMLINUX_SYMBOL_STR(seq_read) },
	{ 0x343a1a8, __VMLINUX_SYMBOL_STR(__list_add) },
	{ 0xfe7c4287, __VMLINUX_SYMBOL_STR(nr_cpu_ids) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x56d697ce, __VMLINUX_SYMBOL_STR(cpu_up) },
	{ 0x479c3c86, __VMLINUX_SYMBOL_STR(find_next_zero_bit) },
	{ 0xc2560ac2, __VMLINUX_SYMBOL_STR(pci_read_config_word) },
	{ 0x9abdea30, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0x521445b, __VMLINUX_SYMBOL_STR(list_del) },
	{ 0x618911fc, __VMLINUX_SYMBOL_STR(numa_node) },
	{ 0xf0fdf6cb, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0x6e8bf789, __VMLINUX_SYMBOL_STR(hrtimer_start) },
	{ 0xebfdcb96, __VMLINUX_SYMBOL_STR(pci_read_config_dword) },
	{ 0x910538ff, __VMLINUX_SYMBOL_STR(pv_cpu_ops) },
	{ 0x9ab8f995, __VMLINUX_SYMBOL_STR(cpumask_next_and) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
	{ 0x41ec4c1a, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0xf99d347e, __VMLINUX_SYMBOL_STR(node_states) },
	{ 0xd94cc09, __VMLINUX_SYMBOL_STR(__per_cpu_offset) },
	{ 0x91c11bc0, __VMLINUX_SYMBOL_STR(get_device) },
	{ 0x8c34c149, __VMLINUX_SYMBOL_STR(proc_create_data) },
	{ 0x1e047854, __VMLINUX_SYMBOL_STR(warn_slowpath_fmt) },
	{ 0x1685c91c, __VMLINUX_SYMBOL_STR(seq_lseek) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x25a97010, __VMLINUX_SYMBOL_STR(hrtimer_init) },
	{ 0xb352177e, __VMLINUX_SYMBOL_STR(find_first_bit) },
	{ 0x58af4a0f, __VMLINUX_SYMBOL_STR(pci_get_device) },
	{ 0xdaf7b334, __VMLINUX_SYMBOL_STR(pci_dev_put) },
	{ 0x77e2f33, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0x6228c21f, __VMLINUX_SYMBOL_STR(smp_call_function_single) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "2FF06F265217ABEC5332E44");
MODULE_INFO(rhelversion, "7.2");
