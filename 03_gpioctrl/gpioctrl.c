#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio/consumer.h>

static struct gpio_desc *led, *button;

#define IO_LED 21
#define IO_BUTTON 20
#define IO_OFFSET 0

static int __init my_init(void)
{
	int status;

	led = gpio_to_desc(IO_LED + IO_OFFSET);
	if (!led) {
		printk("gpioctrl - Error getting pin %d\n", IO_LED);
		return -ENODEV;
	}

	button = gpio_to_desc(IO_BUTTON + IO_OFFSET);
	if (!button) {
		printk("gpioctrl - Error getting pin %d\n", IO_BUTTON);
		return -ENODEV;
	}

	status = gpiod_direction_output(led, 0);
	if (status) {
		printk("gpioctrl - Error setting LED to output\n");
		return status;
	}

	status = gpiod_direction_input(button);
	if (status) {
		printk("gpioctrl - Error setting BUTTON to input\n");
		return status;
	}

	gpiod_set_value(led, 1);
	printk("button is %spressed\n", gpiod_get_value(button) ? "" : "not ");

	return 0;
}

static void __exit my_exit(void)
{
	printk("gpioctrl - Module unloaded\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("h4xrunner");
MODULE_DESCRIPTION("Example GPIO kernel module without Device Tree");
