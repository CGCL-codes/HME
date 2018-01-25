#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
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
	{ 0x50c141e7, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x3375f654, __VMLINUX_SYMBOL_STR(kobject_put) },
	{ 0x4e536271, __VMLINUX_SYMBOL_STR(__dynamic_pr_debug) },
	{ 0xbd415fa, __VMLINUX_SYMBOL_STR(sysfs_create_file_ns) },
	{ 0xb53afbdd, __VMLINUX_SYMBOL_STR(kobject_create_and_add) },
	{ 0x5e10b804, __VMLINUX_SYMBOL_STR(kernel_kobj) },
	{ 0x20c55ae0, __VMLINUX_SYMBOL_STR(sscanf) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x6228c21f, __VMLINUX_SYMBOL_STR(smp_call_function_single) },
	{ 0x9166fada, __VMLINUX_SYMBOL_STR(strncpy) },
	{ 0xd2b09ce5, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0x81e71c99, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0xd2d76643, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x754d539c, __VMLINUX_SYMBOL_STR(strlen) },
	{ 0x91715312, __VMLINUX_SYMBOL_STR(sprintf) },
	{ 0xeae3dfd6, __VMLINUX_SYMBOL_STR(__const_udelay) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "0EEB404F8848DDAA8AA88FA");
