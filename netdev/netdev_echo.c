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
#include <linux/can/dev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("h4xrunner");
MODULE_DESCRIPTION("A robust UART-to-CAN bridge driver (serdev -> vcan0)");

/* Global variables for CAN handling */
static struct net_device *vcan_ndev;

#define UART_RX_BUFFER_SIZE 256
static unsigned char uart_rx_buffer[UART_RX_BUFFER_SIZE];
static size_t uart_rx_buffer_pos;

/* Define a simple protocol for UART-to-CAN conversion */
#define CAN_START_BYTE 0xAA /* Start of a CAN frame on UART */
#define CAN_ID_LEN 2        /* 11-bit CAN ID takes 2 bytes */
#define CAN_DLC_LEN 1       /* DLC takes 1 byte */
#define CAN_HEADER_LEN (1 + CAN_ID_LEN + CAN_DLC_LEN) /* Total header size */

/* Function prototypes */
static int serdev_echo_probe(struct serdev_device *serdev);
static void serdev_echo_remove(struct serdev_device *serdev);

static struct of_device_id serdev_echo_ids[] = {
    { .compatible = "brightlight,echodev", },
    { /* sentinel */ }
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
 * Helper: process internal buffer for full CAN frames and inject to vcan0
 * This assumes uart_rx_buffer contains concatenated UART bytes and uart_rx_buffer_pos is length.
 */
static void process_uart_buffer(void)
{
    size_t i;

    /* While we have at least a header to examine */
    while (uart_rx_buffer_pos >= CAN_HEADER_LEN) {
        /* Check start byte */
        if (uart_rx_buffer[0] != CAN_START_BYTE) {
            printk(KERN_WARNING "netdev_echo - Wrong start byte (0x%02x). Skipping one byte.\n",
                   uart_rx_buffer[0]);
            /* Skip one byte and continue */
            memmove(uart_rx_buffer, uart_rx_buffer + 1, uart_rx_buffer_pos - 1);
            uart_rx_buffer_pos--;
            continue;
        }

        /* We have start byte and at least header bytes present; read DLC safely */
        u8 dlc = uart_rx_buffer[1 + CAN_ID_LEN];

        if (dlc > CAN_MAX_DLC) {
            /*
             * DLC invalid (greater than allowed). This can happen either due to corruption
             * or misalignment. Skip the start byte and try to resync to next possible frame.
             */
            printk(KERN_WARNING "netdev_echo - Invalid DLC value (%u). Skipping start byte to resync.\n", dlc);
            memmove(uart_rx_buffer, uart_rx_buffer + 1, uart_rx_buffer_pos - 1);
            uart_rx_buffer_pos--;
            continue;
        }

        size_t frame_len = CAN_HEADER_LEN + dlc;

        /* If we don't yet have the full frame, wait for more bytes */
        if (uart_rx_buffer_pos < frame_len)
            break;

        /* We have a full frame -> inject to vcan */
        if (vcan_ndev) {
            struct sk_buff *skb;
            struct can_frame *cf;
            u16 can_id_val = (uart_rx_buffer[1] << 8) | uart_rx_buffer[2];

            skb = alloc_can_skb(vcan_ndev, &cf);
            if (skb) {
                /* Mask to 11-bit standard ID (SFF) */
                cf->can_id = can_id_val & CAN_SFF_MASK;
                cf->can_dlc = dlc;
                if (dlc)
                    memcpy(cf->data, &uart_rx_buffer[CAN_HEADER_LEN], dlc);

                printk(KERN_INFO "netdev_echo - Injected CAN Frame (ID:0x%03x, DLC:%u): ",
                       cf->can_id, cf->can_dlc);
                for (i = 0; i < cf->can_dlc; i++)
                    printk(KERN_CONT "%02x ", cf->data[i]);
                printk(KERN_CONT "\n");

                /* rx path: network stack owns skb after netif_rx */
                netif_rx(skb);
            } else {
                printk(KERN_ERR "netdev_echo - Failed to allocate skb for CAN frame.\n");
            }
        } else {
            printk(KERN_WARNING "netdev_echo - vcan_ndev is NULL, CAN frame not sent.\n");
        }

        /* Remove processed frame from buffer */
        if (uart_rx_buffer_pos > frame_len) {
            memmove(uart_rx_buffer, uart_rx_buffer + frame_len, uart_rx_buffer_pos - frame_len);
        }
        uart_rx_buffer_pos -= frame_len;
    }
}

/*
 * This function parses incoming UART data and buffers it until a full CAN frame is available.
 * It returns the number of bytes consumed from the serdev layer (we consume all provided bytes).
 */
static size_t serdev_echo_recv(struct serdev_device *serdev, const unsigned char *buffer, size_t size)
{
    size_t to_copy;

    /* Log the received bytes for debugging purposes */
    printk(KERN_DEBUG "netdev_echo - Received %zu bytes: ", size);
    for (size_t i = 0; i < size; i++)
        printk(KERN_CONT "%02x ", buffer[i]);
    printk(KERN_CONT "\n");

    /* If incoming would overflow our buffer, drop old data to make room (or skip) */
    if (uart_rx_buffer_pos + size > UART_RX_BUFFER_SIZE) {
        printk(KERN_WARNING "serdev_echo - UART RX buffer would overflow. Dropping buffer.\n");
        uart_rx_buffer_pos = 0;
    }

    /* Copy into internal buffer */
    to_copy = min((size_t)size, UART_RX_BUFFER_SIZE - uart_rx_buffer_pos);
    if (to_copy)
        memcpy(uart_rx_buffer + uart_rx_buffer_pos, buffer, to_copy);
    uart_rx_buffer_pos += to_copy;

    /* Process as many frames as possible */
    process_uart_buffer();

    /* Return the number of bytes consumed from serdev (we consumed 'size') */
    return size;
}

static const struct serdev_device_ops serdev_echo_ops = {
    .receive_buf = serdev_echo_recv,
};

static int serdev_echo_probe(struct serdev_device *serdev)
{
    int status;
    int baudrate = 9600;

    printk(KERN_INFO "netdev_echo - probe: initializing\n");

    /* find vcan0 */
    vcan_ndev = dev_get_by_name(&init_net, "vcan0");
    if (!vcan_ndev) {
        printk(KERN_ERR "netdev_echo - 'vcan0' interface not found. Create it with: sudo ip link add dev vcan0 type vcan; sudo ip link set up vcan0\n");
        return -ENODEV;
    }
    printk(KERN_INFO "netdev_echo - 'vcan0' interface found successfully.\n");

    /* initialize our uart buffer state */
    uart_rx_buffer_pos = 0;

    /* register serdev ops and open device */
    serdev_device_set_client_ops(serdev, &serdev_echo_ops);

    status = serdev_device_open(serdev);
    if (status) {
        printk(KERN_ERR "netdev_echo - Error opening serial port: %d\n", status);
        dev_put(vcan_ndev);
        vcan_ndev = NULL;
        return status;
    }

    serdev_device_set_baudrate(serdev, baudrate);
    serdev_device_set_flow_control(serdev, false);
    serdev_device_set_parity(serdev, SERDEV_PARITY_NONE);
    printk(KERN_INFO "netdev_echo - UART Baudrate set to %d\n", baudrate);

    serdev_device_write_buf(serdev,
                            "UART-to-CAN bridge driver initialized.\n",
                            sizeof("UART-to-CAN bridge driver initialized.\n") - 1);

    return 0;
}

static void serdev_echo_remove(struct serdev_device *serdev)
{
    printk(KERN_INFO "netdev_echo - remove: shutting down\n");

    serdev_device_close(serdev);

    if (vcan_ndev) {
        dev_put(vcan_ndev);
        vcan_ndev = NULL;
    }

    /* clear buffer state */
    uart_rx_buffer_pos = 0;
}

static int __init my_init(void)
{
    printk(KERN_INFO "netdev_echo - Loading the driver...\n");
    if (serdev_device_driver_register(&serdev_echo_driver)) {
        printk(KERN_ERR "netdev_echo - Error! Could not load driver\n");
        return -ENODEV;
    }
    return 0;
}

static void __exit my_exit(void)
{
    printk(KERN_INFO "netdev_echo - Unloading driver\n");
    serdev_device_driver_unregister(&serdev_echo_driver);
}

module_init(my_init);
module_exit(my_exit);
