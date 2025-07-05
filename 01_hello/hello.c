#include <linux/module.h>
#include <linux/init.h>


int my_init(void)
{
	printk("hello - Hello, Kernel!\n");
	return 0;
}

int my_exit(void)
{
	printk("hello - Goodbye, Kernel!\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("CAL");
