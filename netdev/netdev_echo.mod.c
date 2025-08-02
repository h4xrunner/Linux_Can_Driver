#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
	{ 0x92997ed8, "_printk" },
	{ 0x8a1e2a70, "__serdev_device_driver_register" },
	{ 0x11118a0c, "serdev_device_close" },
	{ 0xdba01bec, "init_net" },
	{ 0x87fb4df, "dev_get_by_name" },
	{ 0x53ec325d, "serdev_device_open" },
	{ 0xe61fd539, "serdev_device_set_baudrate" },
	{ 0xa917a17d, "serdev_device_set_flow_control" },
	{ 0xbea66a16, "serdev_device_set_parity" },
	{ 0x6b5091a, "serdev_device_write_buf" },
	{ 0x9d669763, "memcpy" },
	{ 0x99bb8806, "memmove" },
	{ 0x78cab047, "alloc_can_skb" },
	{ 0x6363e7ac, "netif_rx" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x23aeac3b, "driver_unregister" },
	{ 0x7d5dce20, "module_layout" },
};

MODULE_INFO(depends, "can-dev");

MODULE_ALIAS("of:N*T*Cbrightlight,echodev");
MODULE_ALIAS("of:N*T*Cbrightlight,echodevC*");

MODULE_INFO(srcversion, "5FB2486CC5E50DB3D69FDC6");
