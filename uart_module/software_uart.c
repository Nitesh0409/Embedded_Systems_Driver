#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h> 

#define MAX_PORTS 2 
#define RX_BUF_SIZE 256
#define TX1_GPIO 590 
#define RX1_GPIO 591
#define TX2_GPIO 592
#define RX2_GPIO 593
#define BAUD_RATE 9600
#define BIT_TIME_NS (1000000000 / BAUD_RATE)

struct soft_uart_port {
    int minor_num;
    struct gpio_desc *tx_gpio;
    struct gpio_desc *rx_gpio;
    int rx_irq;

    struct hrtimer tx_timer;
    struct hrtimer rx_timer;

    unsigned char rx_buffer[RX_BUF_SIZE];
    int rx_head, rx_tail;
    int rx_bit_index;              // Fixed: Added missing variable
    unsigned char rx_current_byte; // Fixed: Added missing variable
    
    unsigned char tx_buffer[RX_BUF_SIZE];
    int tx_head, tx_tail;
    int tx_bit_index;            
    unsigned char tx_current_byte; 
    int tx_busy;                   
    wait_queue_head_t read_wait;
};

static struct soft_uart_port my_ports[MAX_PORTS];
static dev_t dev_num;
static struct cdev uart_cdev;
static struct class *uart_class;

// --- RECEIVE ENGINE ---
static irqreturn_t rx_interrupt_handler(int irq, void *dev_id) {
    struct soft_uart_port *port = (struct soft_uart_port *)dev_id;

    disable_irq_nosync(port->rx_irq);
    port->rx_bit_index = 0;
    port->rx_current_byte = 0;

    // Start timer at 1.5x bit time to hit the middle of Bit 0 
    hrtimer_start(&port->rx_timer, ktime_set(0, (BIT_TIME_NS * 3) / 2), HRTIMER_MODE_REL);
    
    return IRQ_HANDLED;
}

static enum hrtimer_restart rx_timer_callback(struct hrtimer *timer) {
    struct soft_uart_port *port = container_of(timer, struct soft_uart_port, rx_timer);
    int bit_val = gpiod_get_value(port->rx_gpio); 

    if (port->rx_bit_index < 8) {
        if (bit_val) 
            port->rx_current_byte |= (1 << port->rx_bit_index);
        else 
            port->rx_current_byte &= ~(1 << port->rx_bit_index);

        port->rx_bit_index++;
        hrtimer_forward_now(timer, ktime_set(0, BIT_TIME_NS));
        return HRTIMER_RESTART;
    } 
    
    // Stop bit reached - Store the byte
    port->rx_buffer[port->rx_head] = port->rx_current_byte;
    port->rx_head = (port->rx_head + 1) % RX_BUF_SIZE;
    
    printk(KERN_INFO "Soft UART Port %d received: %c\n", port->minor_num + 1, port->rx_current_byte);

    // Wake up any user space process (like 'cat') waiting for data
    wake_up_interruptible(&port->read_wait);
    enable_irq(port->rx_irq);
    
    return HRTIMER_NORESTART; // Fixed: Unconditional return
}

// --- TRANSMIT ENGINE ---
static enum hrtimer_restart tx_timer_callback(struct hrtimer *timer) {
    struct soft_uart_port *port = container_of(timer, struct soft_uart_port, tx_timer);
    ktime_t now;

    if (port->tx_bit_index == 0) {
        gpiod_set_value(port->tx_gpio, 0); // START BIT
    }
    else if (port->tx_bit_index >= 1 && port->tx_bit_index <= 8) {
        int bit_val = (port->tx_current_byte >> (port->tx_bit_index - 1)) & 0x01;
        gpiod_set_value(port->tx_gpio, bit_val); // DATA BITS
    }
    else if (port->tx_bit_index == 9) {
        gpiod_set_value(port->tx_gpio, 1); // STOP BIT
    }
    
    port->tx_bit_index++;

    if (port->tx_bit_index > 9) {
        port->tx_tail = (port->tx_tail + 1) % RX_BUF_SIZE; 
        if (port->tx_head != port->tx_tail) {
            port->tx_bit_index = 0;
            port->tx_current_byte = port->tx_buffer[port->tx_tail];
        } else {
            port->tx_busy = 0;
            return HRTIMER_NORESTART; 
        }
    }
    
    now = hrtimer_cb_get_time(timer);
    hrtimer_forward(timer, now, ktime_set(0, BIT_TIME_NS));
    return HRTIMER_RESTART;
}

// --- FILE OPERATIONS ---
static ssize_t device_write(struct file *fp, const char __user *buffer, size_t length, loff_t *offset) {
    int minor = iminor(fp->f_inode);
    struct soft_uart_port *port = &my_ports[minor];
    int bytes_written = 0;
    char temp_char;

    // LOUD DEBUG: Tell dmesg we received a write command
    printk(KERN_INFO "Soft UART: Write called on Port %d, sending %zu bytes\n", minor + 1, length);

    while (bytes_written < length) {
        int next_head = (port->tx_head + 1) % RX_BUF_SIZE;
        if (next_head == port->tx_tail) break;

        if (copy_from_user(&temp_char, buffer + bytes_written, 1)) {
            return -EFAULT;
        }
        port->tx_buffer[port->tx_head] = temp_char;
        
        // --- SOFTWARE LOOPBACK HACK ---
        int other_minor = (minor == 0) ? 1 : 0; // Find the other port
        struct soft_uart_port *other_port = &my_ports[other_minor];
        
        other_port->rx_buffer[other_port->rx_head] = temp_char; // Copy directly to other port's RX
        other_port->rx_head = (other_port->rx_head + 1) % RX_BUF_SIZE;
        wake_up_interruptible(&other_port->read_wait); // Wake up the 'cat' command!
        
        // LOUD DEBUG: Tell dmesg the loopback worked
        printk(KERN_INFO "Soft UART: Port %d Loopback sent '%c' to Port %d\n", minor + 1, temp_char, other_minor + 1);
        // ------------------------------

        port->tx_head = next_head;
        bytes_written++;
    }
    
    // Kick off the physical TX pins anyway (even though they aren't wired)
    if (!port->tx_busy && port->tx_head != port->tx_tail) {
        port->tx_busy = 1;
        port->tx_bit_index = 0; 
        port->tx_current_byte = port->tx_buffer[port->tx_tail];
        hrtimer_start(&port->tx_timer, ktime_set(0, BIT_TIME_NS), HRTIMER_MODE_REL);
    }
    
    return bytes_written;
}
// NEW: Read function to allow user space to read received data
static ssize_t device_read(struct file *fp, char __user *buffer, size_t length, loff_t *offset) {
    int minor = iminor(fp->f_inode);
    struct soft_uart_port *port = &my_ports[minor];
    int bytes_read = 0;
    int ret;

    // Sleep until the RX buffer has data
    ret = wait_event_interruptible(port->read_wait, port->rx_head != port->rx_tail);
    if (ret) return ret; // Exit if interrupted (e.g., user pressed Ctrl+C)

    // Send the data up to User Space
    while (bytes_read < length && port->rx_head != port->rx_tail) {
        if (copy_to_user(buffer + bytes_read, &port->rx_buffer[port->rx_tail], 1)) {
            return -EFAULT;
        }
        port->rx_tail = (port->rx_tail + 1) % RX_BUF_SIZE;
        bytes_read++;
    }

    return bytes_read;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .write = device_write,
    .read = device_read, // Fixed: Added read operation
};

// --- INIT & EXIT ---
static int __init soft_uart_init(void) {
    int i, ret;
    int tx_pins[MAX_PORTS] = {TX1_GPIO, TX2_GPIO};
    int rx_pins[MAX_PORTS] = {RX1_GPIO, RX2_GPIO};

    printk(KERN_INFO "Soft UART: Initializing the UART Module ......\n");

    ret = alloc_chrdev_region(&dev_num, 0, MAX_PORTS, "soft_uart");
    if (ret < 0) return ret;

    cdev_init(&uart_cdev, &fops);
    ret = cdev_add(&uart_cdev, dev_num, MAX_PORTS);
    if (ret < 0) {
        unregister_chrdev_region(dev_num, MAX_PORTS);
        return ret;
    }

    uart_class = class_create("soft_uart_class");
    if (IS_ERR(uart_class)) {
        cdev_del(&uart_cdev);
        unregister_chrdev_region(dev_num, MAX_PORTS);
        return PTR_ERR(uart_class);
    }

    for (i = 0; i < MAX_PORTS; i++) {
        hrtimer_init(&my_ports[i].tx_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        my_ports[i].tx_timer.function = tx_timer_callback;
        
        hrtimer_init(&my_ports[i].rx_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        my_ports[i].rx_timer.function = rx_timer_callback;

        gpio_request(tx_pins[i], "soft_uart_tx");
        gpio_direction_output(tx_pins[i], 1); 
        my_ports[i].tx_gpio = gpio_to_desc(tx_pins[i]);
        
        gpio_request(rx_pins[i], "soft_uart_rx");
        gpio_direction_input(rx_pins[i]);
        my_ports[i].rx_gpio = gpio_to_desc(rx_pins[i]);

        my_ports[i].rx_irq = gpio_to_irq(rx_pins[i]);
        
        // Fixed: Catching return value of request_irq
        ret = request_irq(my_ports[i].rx_irq, rx_interrupt_handler, IRQF_TRIGGER_FALLING, "soft_uart_rx_irq", &my_ports[i]);
        if (ret) {
            printk(KERN_ERR "Soft UART Port %d: Failed to request IRQ\n", i + 1);
        }

        my_ports[i].minor_num = i;
        my_ports[i].tx_head = 0; my_ports[i].tx_tail = 0; my_ports[i].tx_busy = 0;
        my_ports[i].rx_head = 0; my_ports[i].rx_tail = 0;
        init_waitqueue_head(&my_ports[i].read_wait);
        
        device_create(uart_class, NULL, MKDEV(MAJOR(dev_num), i), NULL, "soft_uart%d", i + 1);
    }
    
    return 0;
}

static void __exit soft_uart_exit(void) {
    int i;
    for (i = 0; i < MAX_PORTS; i++) {
        hrtimer_cancel(&my_ports[i].tx_timer);
        hrtimer_cancel(&my_ports[i].rx_timer);
        
        free_irq(my_ports[i].rx_irq, &my_ports[i]);
        gpio_free(desc_to_gpio(my_ports[i].tx_gpio));
        gpio_free(desc_to_gpio(my_ports[i].rx_gpio));
        device_destroy(uart_class, MKDEV(MAJOR(dev_num), i));
    }

    class_destroy(uart_class);
    cdev_del(&uart_cdev);
    unregister_chrdev_region(dev_num, MAX_PORTS);

    printk(KERN_INFO "Soft UART: Module successfully unloaded.\n");
}

module_init(soft_uart_init);
module_exit(soft_uart_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Aaditya Biswas");
MODULE_DESCRIPTION("Dual Software UART Driver");
