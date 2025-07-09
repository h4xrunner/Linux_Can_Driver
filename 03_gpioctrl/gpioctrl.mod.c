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
	{ 0xc5d2a899, "gpiod_set_value" },
	{ 0x97dc172d, "gpio_to_desc" },
	{ 0xf02242bb, "gpiod_direction_output" },
	{ 0x53a69954, "gpiod_direction_input" },
	{ 0xfe6b981f, "gpiod_get_value" },
	{ 0x122c3a7e, "_printk" },
	{ 0x39ff040a, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "2E53C9DA038850F28E17B10");
