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
	{ 0x16a5a12f, __VMLINUX_SYMBOL_STR(single_open) },
	{ 0x930484aa, __VMLINUX_SYMBOL_STR(cpu_online_mask) },
	{ 0x2296f507, __VMLINUX_SYMBOL_STR(single_release) },
	{ 0xc0a3d105, __VMLINUX_SYMBOL_STR(find_next_bit) },
	{ 0x74df1d4, __VMLINUX_SYMBOL_STR(seq_printf) },
	{ 0xa16aae11, __VMLINUX_SYMBOL_STR(remove_proc_entry) },
	{ 0x4ed12f73, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0x54efb5d6, __VMLINUX_SYMBOL_STR(cpu_number) },
	{ 0x9c3df9b4, __VMLINUX_SYMBOL_STR(seq_read) },
	{ 0xfe7c4287, __VMLINUX_SYMBOL_STR(nr_cpu_ids) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x88b04e39, __VMLINUX_SYMBOL_STR(__register_nmi_handler) },
	{ 0x9abdea30, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0x618911fc, __VMLINUX_SYMBOL_STR(numa_node) },
	{ 0xf0fdf6cb, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
	{ 0xd94cc09, __VMLINUX_SYMBOL_STR(__per_cpu_offset) },
	{ 0x8c34c149, __VMLINUX_SYMBOL_STR(proc_create_data) },
	{ 0x1685c91c, __VMLINUX_SYMBOL_STR(seq_lseek) },
	{ 0xe64ad8ea, __VMLINUX_SYMBOL_STR(unregister_nmi_handler) },
	{ 0x512101d1, __VMLINUX_SYMBOL_STR(apic) },
	{ 0x77e2f33, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0x6228c21f, __VMLINUX_SYMBOL_STR(smp_call_function_single) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "CBD9D750CB2F3B1DB288306");
MODULE_INFO(rhelversion, "7.2");
