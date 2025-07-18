#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>       
#include <linux/kernel.h>

#define IO_LED     18
#define IO_BUTTON  20

static int __init my_init(void)
{
    int status;

    
    status = gpio_request(IO_LED, "led");
    if (status) {
        printk(KERN_ERR "gpioctrl - Error requesting LED GPIO %d\n", IO_LED);
        return status;
    }

    
    status = gpio_request(IO_BUTTON, "button");
    if (status) {
        printk(KERN_ERR "gpioctrl - Error requesting BUTTON GPIO %d\n", IO_BUTTON);
        gpio_free(IO_LED);
        return status;
    }

    
    status = gpio_direction_output(IO_LED, 0);
    if (status) {
        printk(KERN_ERR "gpioctrl - Error setting LED direction\n");
        gpio_free(IO_LED);
        gpio_free(IO_BUTTON);
        return status;
    }

    
    status = gpio_direction_input(IO_BUTTON);
    if (status) {
        printk(KERN_ERR "gpioctrl - Error setting BUTTON direction\n");
        gpio_free(IO_LED);
        gpio_free(IO_BUTTON);
        return status;
    }

   
    gpio_set_value(IO_LED, 1);

    
    printk(KERN_INFO "button is %spressed\n", gpio_get_value(IO_BUTTON) ? "" : "not ");

    return 0;
}

static void __exit my_exit(void)
{
  
    gpio_set_value(IO_LED, 0);

    
    gpio_free(IO_LED);
    gpio_free(IO_BUTTON);

    printk(KERN_INFO "gpioctrl - Module unloaded\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("h4xrunner");
MODULE_DESCRIPTION("GPIO control example without device tree");
