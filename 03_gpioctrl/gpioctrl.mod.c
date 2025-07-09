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
	{ 0x47229b5c, "gpio_request" },
	{ 0xd9ec4b80, "gpio_to_desc" },
	{ 0xf0024008, "gpiod_direction_output_raw" },
	{ 0x67d1affd, "gpiod_direction_input" },
	{ 0x178f2652, "gpiod_set_raw_value" },
	{ 0x74f08e6f, "gpiod_get_raw_value" },
	{ 0x122c3a7e, "_printk" },
	{ 0xfe990052, "gpio_free" },
	{ 0x39ff040a, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "C61846241567BA38D77B61D");
