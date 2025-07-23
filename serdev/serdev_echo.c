#include <linux/module.h>
#include <linux/init.h>
#include <linux/serdev.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("h4xrunner");
MODULE_DESCRIPTION("A simple loopback driver for UART port");

/* Declate the probe and remove functions */
static int serdev_echo(struct serdev_device *serdev);
static int void serdev_remove(struct serdev_device *serdev);

static struct of_device_id serdev_echo_ids[] = {
	{
		.compatible = "brightlight,echodev",
	}, { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, serdev_echo_ids);

static struct serdev-device_driver serdev_echo_driver = {
	.probe = serdev_echo,
	.remove = dt_remove,
	.driver = {
		.name = "my_device_driver",
		.of_match_table = serdev_echo_ids,
	},
};

static int serdev_echo_recv(struct serdev_device *serdev, const unsigned char *buffer, size_t size){
	printk("serdev_echo - Received %ld bytes with \"\"\n", size, buffer);
	return serdev_device_write_buf(struct serdev_device *serdev);
}

static const struct serdev_device_ops serdev_echo_ops{
	.receive_buffer = serdev_echo_recv,
};
/**
 * @brief This function is called on loading the driver 
 */
static int serdev_echo_probe(struct serdev_device *serdev) {
	int status;
	printk("serdev_echo - Now I am in the probe function!\n");
	
	serdev_device-set_client_ops(serdev, &serdev_echo_ops);
	status = serdev_device_open(serdev);
	if(status){
		printk("serdev_echo - Error opening serial port!\n");
		return -status;
	}
	
	serdev_device_set_baudrate(serdev, 9600);
	serdev_device_set_flow_control(serdev, false);
	serdev_device_set_parity(serdev, SERDEV_PARITY_NONE);
	return 0;
}

/**
 * @brief This function is called on unloading the driver 
 */
static void serdev_echo_remove(struct serdev_device *serdev) {
	printk("serdev_echo - Now I am in the remove function\n");
	return 0;
}

/**
 * @brief This function is called, when the module is loaded into the kernel
 */
static int __init my_init(void) {
	printk("serdev_echo - Loading the driver...\n");
	if(platform_driver_register(&my_driver)) {
		printk("serdev_echo - Error! Could not load driver\n");
		return -1;
	}
	return 0;
}

/**
 * @brief This function is called, when the module is removed from the kernel
 */
static void __exit my_exit(void) {
	printk("serdev_echo - Unload driver");
	platform_driver_unregister(&my_driver);
}

module_init(my_init);
module_exit(my_exit);
