#include <linux/module.h>
#include <linux/init.h>
#include <linux/serdev.h>
#include <linux/mod_devicetable.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/can.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/net_namespace.h> // init_net

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("h4xrunner");
MODULE_DESCRIPTION("A simple UART-to-CAN bridge driver");

/* Global variables for CAN handling */
static struct net_device *vcan_ndev;
#define UART_RX_BUFFER_SIZE 64
static unsigned char uart_rx_buffer[UART_RX_BUFFER_SIZE];
static size_t uart_rx_buffer_pos = 0;

/* Define a simple protocol for UART-to-CAN conversion */
#define CAN_START_BYTE 0xAA // Start of a CAN frame on UART
#define CAN_ID_LEN 2        // 11-bit CAN ID takes 2 bytes
#define CAN_DLC_LEN 1       // DLC takes 1 byte
#define CAN_HEADER_LEN (1 + CAN_ID_LEN + CAN_DLC_LEN) // Total header size

/* Function prototypes */
static int serdev_echo_probe(struct serdev_device *serdev);
static void serdev_echo_remove(struct serdev_device *serdev);

static struct of_device_id serdev_echo_ids[] = {
    {
        .compatible = "brightlight,echodev",
    }, { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, serdev_echo_ids);

static struct serdev_device_driver serdev_echo_driver = {
    .probe = serdev_echo_probe,
    .remove = serdev_echo_remove,
    .driver = {
        .name = "serdev-echo",
        .of_match_table = serdev_echo_ids,
    },
};

/*
 * @brief This function parses incoming UART data and injects it into the vcan interface.
 */
static size_t serdev_echo_recv(struct serdev_device *serdev, const unsigned char *buffer, size_t size){
    int i;
    size_t bytes_processed = 0;

    // Log the received bytes for debugging purposes
    printk(KERN_INFO "serdev_echo - Received %ld bytes: ", size); // %ld isteğe göre değişmedi
    for (i = 0; i < size; i++) {
        printk(KERN_CONT "%02x ", buffer[i]);
    }
    printk(KERN_CONT "\n");

    // Copy received data to our internal buffer
    if (uart_rx_buffer_pos + size > UART_RX_BUFFER_SIZE) {
        printk(KERN_WARNING "serdev_echo - UART RX buffer overflow! Data might be lost.\n");
        uart_rx_buffer_pos = 0;
    }
    memcpy(uart_rx_buffer + uart_rx_buffer_pos, buffer, size);
    uart_rx_buffer_pos += size;

    // Process the buffer to find and inject complete CAN frames
    while (uart_rx_buffer_pos >= CAN_HEADER_LEN) {
        if (uart_rx_buffer[0] != CAN_START_BYTE) {
            printk(KERN_WARNING "serdev_echo - Wrong start byte (0x%02x). Skipping one byte.\n", uart_rx_buffer[0]);
            memmove(uart_rx_buffer, uart_rx_buffer + 1, uart_rx_buffer_pos - 1);
            uart_rx_buffer_pos--;
            continue;
        }

        // Get DLC and check if the full frame is in the buffer
        u8 dlc = uart_rx_buffer[1 + CAN_ID_LEN];
        size_t frame_len = CAN_HEADER_LEN + dlc;

        if (dlc > CAN_MAX_DLC) {
            printk(KERN_WARNING "serdev_echo - Invalid DLC value (%d). Skipping frame.\n", dlc);
            uart_rx_buffer_pos = 0; // Clear the buffer to avoid further issues
            break;
        }

        if (uart_rx_buffer_pos < frame_len) {
            // Not enough data for a full frame yet. Wait for more.
            break;
        }

        // We have a full frame in the buffer. Process it.
        if (vcan_ndev) {
            struct sk_buff *skb;
            struct can_frame *cf;
            u16 can_id_val = (uart_rx_buffer[1] << 8) | uart_rx_buffer[2];

            skb = alloc_can_skb(vcan_ndev, &cf);
            if (skb) {
                cf->can_id = can_id_val;
                cf->can_dlc = dlc;
                memcpy(cf->data, &uart_rx_buffer[CAN_HEADER_LEN], dlc);

                printk(KERN_INFO "serdev_echo - Injected CAN Frame (ID:0x%03X, DLC:%d): ", cf->can_id, cf->can_dlc);
                for (i = 0; i < cf->can_dlc; i++) {
                    printk(KERN_CONT "%02x ", cf->data[i]);
                }
                printk(KERN_CONT "\n");

                netif_rx(skb); // Inject frame into vcan interface
                bytes_processed += frame_len;
            } else {
                printk(KERN_ERR "serdev_echo - Failed to allocate skb for CAN frame.\n");
            }
        } else {
            printk(KERN_WARNING "serdev_echo - vcan_ndev is NULL, CAN frame not sent.\n");
        }

        // Remove the processed frame from the buffer
        memmove(uart_rx_buffer, uart_rx_buffer + frame_len, uart_rx_buffer_pos - frame_len);
        uart_rx_buffer_pos -= frame_len;
    }

    return 0; // Return 0 to indicate we've handled the incoming bytes
}

static const struct serdev_device_ops serdev_echo_ops = {
    .receive_buf = serdev_echo_recv,
};

/*
 * @brief This function is called on loading the driver
 */
static int serdev_echo_probe(struct serdev_device *serdev) {
    int status;
    printk(KERN_INFO "serdev_echo - Now in the probe function!\n");

    vcan_ndev = dev_get_by_name(&init_net, "vcan0");
    if (!vcan_ndev) {
        printk(KERN_ERR "serdev_echo - 'vcan0' interface not found. Did you create it with `sudo ip link`?\n");
        return -ENODEV;
    }
    printk(KERN_INFO "serdev_echo - 'vcan0' interface found successfully.\n");

    serdev_device_set_client_ops(serdev, &serdev_echo_ops);
    status = serdev_device_open(serdev);
    if(status){
        printk(KERN_ERR "serdev_echo - Error opening serial port!\n");
        dev_put(vcan_ndev);
        return -status;
    }

    serdev_device_set_baudrate(serdev, 230400);
    serdev_device_set_flow_control(serdev, false);
    serdev_device_set_parity(serdev, SERDEV_PARITY_NONE);

    serdev_device_write_buf(serdev, "UART-to-CAN bridge driver initialized.\n", sizeof("UART-to-CAN bridge driver initialized.\n"));
    return 0;
}

/*
 * @brief This function is called on unloading the driver
 */
static void serdev_echo_remove(struct serdev_device *serdev) {
    printk(KERN_INFO "serdev_echo - Now in the remove function\n");
    serdev_device_close(serdev);
    if (vcan_ndev) {
        dev_put(vcan_ndev);
        vcan_ndev = NULL;
    }
}

/*
 * @brief This function is called, when the module is loaded into the kernel
 */
static int __init my_init(void) {
    printk(KERN_INFO "serdev_echo - Loading the driver...\n");
    if (serdev_device_driver_register(&serdev_echo_driver)) {
        printk(KERN_ERR "serdev_echo - Error! Could not load driver\n");
        return -1;
    }
    return 0;
}

/*
 * @brief This function is called, when the module is removed from the kernel
 */
static void __exit my_exit(void) {
    printk(KERN_INFO "serdev_echo - Unloading driver\n");
    serdev_device_driver_unregister(&serdev_echo_driver);
}

module_init(my_init);
module_exit(my_exit);
