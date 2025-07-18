#include <linux/module.h>
#include <linux/init.h>


static int __init my_init(void)
{
	printk(KERN_INFO "log_level - This an info!\n");
	printk(KERN_WARNING "log_level - This an info!\n");
	printk(KERN_ALERT "log_level - This an info!\n");
	printk(KERN_DEBUG "log_level - This an info!\n");
	return 0;
}	

static void __exit my_exit(void)
{
	pr_emerg("log_level - Module is unloaded!\n");
	pr_err("log_level - Error!");
	pr_info("log_level - The last info!");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("CAL");
MODULE_AUTHOR("h4xrunner");
MODULE_DESCRIPTION(" Kernel Log Level driver");
