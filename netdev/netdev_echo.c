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
MODULE_AUTHOR("h4xrunner + GPT");
MODULE_DESCRIPTION("A robust UART-to-CAN bridge driver (serdev <-> vcan0)");

/* Global variables for CAN handling */
static struct net_device *vcan_ndev;
static struct serdev_device *global_serdev; // UART TX için

#define UART_RX_BUFFER_SIZE 256
static unsigned char uart_rx_buffer[UART_RX_BUFFER_SIZE];
static size_t uart_rx_buffer_pos;

/* UART-to-CAN protocol constants */
#define CAN_START_BYTE 0xAA
#define CAN_ID_LEN 2
#define CAN_DLC_LEN 1
#define CAN_HEADER_LEN (1 + CAN_ID_LEN + CAN_DLC_LEN)

/* Original net_device_ops pointer for vcan0 */
static const struct net_device_ops *orig_ndo;

/* Forward declarations */
static int serdev_echo_probe(struct serdev_device *serdev);
static void serdev_echo_remove(struct serdev_device *serdev);

/* Device match table */
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

/* vcan0 transmit hook: cansend ile gelen CAN frame UART'a dönüştürülür */
static netdev_tx_t vcan_uart_xmit(struct sk_buff *skb, struct net_device *dev)
{
    struct can_frame *cf;
    unsigned char tx_buf[CAN_HEADER_LEN + CAN_MAX_DLEN];
    size_t tx_len;

    if (can_dropped_invalid_skb(dev, skb)) {
        printk(KERN_WARNING "netdev_echo - Dropped invalid skb on transmit\n");
        dev_kfree_skb(skb);
        return NETDEV_TX_OK;
    }

    cf = (struct can_frame *)skb->data;

    if (!global_serdev) {
        printk(KERN_WARNING "netdev_echo - global_serdev is NULL, cannot send UART data\n");
        dev_kfree_skb(skb);
        return NETDEV_TX_OK;
    }

    /* UART frame format: start byte + 2 byte CAN ID + DLC + data */
    tx_buf[0] = CAN_START_BYTE;
    tx_buf[1] = (cf->can_id >> 8) & 0xFF;
    tx_buf[2] = cf->can_id & 0xFF;
    tx_buf[3] = cf->can_dlc;
    if (cf->can_dlc)
        memcpy(&tx_buf[4], cf->data, cf->can_dlc);

    tx_len = CAN_HEADER_LEN + cf->can_dlc;

    serdev_device_write_buf(global_serdev, tx_buf, tx_len);

    printk(KERN_INFO "netdev_echo - Sent CAN frame over UART (ID:0x%03X, DLC:%u): ",
           cf->can_id & CAN_SFF_MASK, cf->can_dlc);
    for (size_t i = 0; i < cf->can_dlc; i++) {
        printk(KERN_CONT "%02x ", cf->data[i]);
    }
    printk(KERN_CONT "\n");

    /* free skb since we handled it */
    dev_kfree_skb(skb);
    return NETDEV_TX_OK;
}

/* UART RX buffer processing: UART -> CAN injection */
static void process_uart_buffer(void)
{
    while (uart_rx_buffer_pos >= CAN_HEADER_LEN) {
        if (uart_rx_buffer[0] != CAN_START_BYTE) {
            printk(KERN_WARNING "netdev_echo - Wrong start byte (0x%02x). Skipping one byte.\n", uart_rx_buffer[0]);
            memmove(uart_rx_buffer, uart_rx_buffer + 1, uart_rx_buffer_pos - 1);
            uart_rx_buffer_pos--;
            continue;
        }

        u8 dlc = uart_rx_buffer[1 + CAN_ID_LEN];
        if (dlc > CAN_MAX_DLC) {
            printk(KERN_WARNING "netdev_echo - Invalid DLC value (%u). Skipping start byte.\n", dlc);
            memmove(uart_rx_buffer, uart_rx_buffer + 1, uart_rx_buffer_pos - 1);
            uart_rx_buffer_pos--;
            continue;
        }

        size_t frame_len = CAN_HEADER_LEN + dlc;
        if (uart_rx_buffer_pos < frame_len) {
            /* Tam frame yok, daha fazla veri bekle */
            break;
        }

        if (vcan_ndev) {
            struct sk_buff *skb;
            struct can_frame *cf;
            u16 can_id_val = (uart_rx_buffer[1] << 8) | uart_rx_buffer[2];

            skb = alloc_can_skb(vcan_ndev, &cf);
            if (skb) {
                cf->can_id = can_id_val & CAN_SFF_MASK;
                cf->can_dlc = dlc;
                if (dlc)
                    memcpy(cf->data, &uart_rx_buffer[CAN_HEADER_LEN], dlc);

                printk(KERN_INFO "netdev_echo - Injected CAN Frame (ID:0x%03X, DLC:%u): ",
                       cf->can_id, cf->can_dlc);
                for (size_t i = 0; i < cf->can_dlc; i++)
                    printk(KERN_CONT "%02x ", cf->data[i]);
                printk(KERN_CONT "\n");

                netif_rx(skb);
            } else {
                printk(KERN_ERR "netdev_echo - Failed to allocate skb for CAN frame.\n");
            }
        } else {
            printk(KERN_WARNING "netdev_echo - vcan_ndev is NULL, CAN frame not sent.\n");
        }

        if (uart_rx_buffer_pos > frame_len)
            memmove(uart_rx_buffer, uart_rx_buffer + frame_len, uart_rx_buffer_pos - frame_len);
        uart_rx_buffer_pos -= frame_len;
    }
}

static size_t serdev_echo_recv(struct serdev_device *serdev, const unsigned char *buffer, size_t size)
{
    printk(KERN_DEBUG "netdev_echo - Received %zu bytes: ", size);
    for (size_t i = 0; i < size; i++) {
        printk(KERN_CONT "%02x ", buffer[i]);
    }
    printk(KERN_CONT "\n");

    if (uart_rx_buffer_pos + size > UART_RX_BUFFER_SIZE) {
        printk(KERN_WARNING "serdev_echo - UART RX buffer would overflow. Clearing buffer.\n");
        uart_rx_buffer_pos = 0;
    }

    size_t to_copy = min(size, UART_RX_BUFFER_SIZE - uart_rx_buffer_pos);
    if (to_copy)
        memcpy(uart_rx_buffer + uart_rx_buffer_pos, buffer, to_copy);
    uart_rx_buffer_pos += to_copy;

    process_uart_buffer();

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

    vcan_ndev = dev_get_by_name(&init_net, "vcan0");
    if (!vcan_ndev) {
        printk(KERN_ERR "netdev_echo - 'vcan0' interface not found. Did you create it?\n");
        return -ENODEV;
    }
    printk(KERN_INFO "netdev_echo - 'vcan0' interface found successfully.\n");

    /* Save original netdev_ops */
    orig_ndo = vcan_ndev->netdev_ops;

    /* Clone original ops and override ndo_start_xmit */
    {
        struct net_device_ops *new_ops = kmemdup(orig_ndo, sizeof(*orig_ndo), GFP_KERNEL);
        if (!new_ops) {
            printk(KERN_ERR "netdev_echo - kmemdup failed for netdev_ops\n");
            dev_put(vcan_ndev);
            return -ENOMEM;
        }
        new_ops->ndo_start_xmit = vcan_uart_xmit;
        vcan_ndev->netdev_ops = new_ops;
    }

    serdev_device_set_client_ops(serdev, &serdev_echo_ops);

    status = serdev_device_open(serdev);
    if (status) {
        printk(KERN_ERR "netdev_echo - Error opening serial port: %d\n", status);
        dev_put(vcan_ndev);
        return status;
    }

    serdev_device_set_baudrate(serdev, baudrate);
    serdev_device_set_flow_control(serdev, false);
    serdev_device_set_parity(serdev, SERDEV_PARITY_NONE);
    printk(KERN_INFO "netdev_echo - UART Baudrate set to %d\n", baudrate);

    uart_rx_buffer_pos = 0;
    global_serdev = serdev;

    return 0;
}

static void serdev_echo_remove(struct serdev_device *serdev)
{
    printk(KERN_INFO "netdev_echo - remove: shutting down\n");

    serdev_device_close(serdev);

    if (vcan_ndev) {
        /* Restore original netdev_ops */
        if (orig_ndo) {
            kfree((void *)vcan_ndev->netdev_ops);
            vcan_ndev->netdev_ops = orig_ndo;
        }
        dev_put(vcan_ndev);
        vcan_ndev = NULL;
    }

    global_serdev = NULL;
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
