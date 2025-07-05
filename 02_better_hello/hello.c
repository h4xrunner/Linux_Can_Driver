#include <linux/module.h>
#include <linux/init.h>


int __init my_init(void)
{
	printk("hello - Hello, Kernel!\n");
	return 0;
}

void __exit my_exit(void)
{
	printk("hello - Goodbye, Kernel!\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("CAL");
MODULE_AUTHOR("h4xrunner");
MODULE_DESCRIPTION("A simple Hello Word Linux Kernel module");
