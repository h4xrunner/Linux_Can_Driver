#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
static int major;
static struct file_operations fops;
static int __init my_init(void)
{
	major = register_chrdev(64, "hello_cdev", &fops);	
	if(major <0){
		printk("hello_cdev - Error registering chrdev\n");
		return major;
	}
	printk("hello_cdev - Major Device Number: &d\n", major);
	return 0;
}

static void __exit my_exit(void)
{
	unregister_chrdev(major, "hello_cdev");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("CAL");
MODULE_AUTHOR("h4xrunner");
MODULE_DESCRIPTION("A simple driver for registering a character device");
