#include <linux/hrtimer.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>


// Define your global GPIO numbers based upon i.MX91's specific gpiochip



#define MAX_PORTS 2 
#define RX_BUF_SIZE 256
#define TX1_GPIO 590 
#define RX1_GPIO 591
#define TX2_GPIO 592
#define RX2_GPIO 593


// Hardware Interrupt Service Routine 
static irqreturn_t rx_interrupt_handler(int irq , void * dev_id) {

    struct soft_uart_port * port = (struct  soft_uart_port * ) dev_id;

    printk(KERN_INFO "Soft UART Port %d: START BIT DETECTED!\n", port->minor_num);

    return IRQ_HANDLED;

}








// For single UART Port 
struct soft_uart_port {
  int minor_num;

  struct gpio_desc * tx_gpio;
  struct gpio_desc * rx_gpio;
  int rx_irq;

  // Timers 
  struct hrtimer tx_timer;
  struct hrtimer rx_timer;

  // Buffers and State 
  unsigned char rx_buffer[RX_BUF_SIZE];
  int rx_head;
  int rx_tail;

  wait_queue_head_t read_wait;
};

// Create two independent ports 
static struct soft_uart_port my_ports[MAX_PORTS];

static dev_t dev_num;
static struct cdev uart_cdev;
static struct class * uart_class;

// Forward declatations 
static ssize_t device_read(struct file * fp, char __user * buffer, size_t length , loff_t * offset);
static ssize_t device_write(struct file * fp, const char __user * buffer, size_t length ,loff_t * offset);

static struct file_operations fops = {
  .owner = THIS_MODULE;
  // .read = 
  // .write = 
};







static int __init soft_uart_init(void) {
    int i, ret;

    printk(KERN_INFO "Soft UART: Initializing the UART Module ......");

    ret = alloc_chrdev_region(&dev_num , 0 ,MAX_PORTS , "soft_uart");
    if (ret < 0) {
        printk(KERN_ERR "Soft UART: Failed to allocate major number");
        return ret;

    }
    cdev_init(&uart_cdev , &fops);
    ret = cdev_add(&uart_cdev,dev_num,MAX_PORTS);
    if (ret < 0) {
        printk(KERN_ERR "Soft UART: Failed to add cdev");
        unregister_chrdev_region(dev_num , MAX_PORTS);
        return ret;
    }
    uart_class = class_create("soft_uart_class");
    if (IS_ERR(uart_class)) {
        cdev_del(&uart_cdev);
        unregister_chrdev_region(dev_num,MAX_PORTS);
        return PTR_ERR(uart_class);
    }
    int tx_pins[MAX_PORTS] = {TX1_GPIO , TX2_GPIO};
    int rx_pins[MAX_PORTS] = {RX1_GPIO ,RX2_GPIO};


    // Initialize ports 
    for (i = 0 ; i < MAX_PORTS ; i++) {
        // Claim and setup TX pin 
        ret = gpio_request(tx_pins[i],"soft_uart_tx");
        
        if (ret ) {
            printk(KERN_ERR "SOFT UART : Failed to request TX GPIO %d\n",tx_pins[i]);
            return ret;
        }
        gpio_direction_output(tx_pins[i], 1); // 1 = HIGH
        my_ports[i].tx_gpio = gpio_to_desc(tx_pins[i]);
        ret  = gpio_request(rx_pins[i], "soft_uart_rx");
        if (ret) {
            printk(KERN_ERR "Software UART: Failed to request RX GPIO %d\n",rx_pins[i]);
            return ret;
        }
        gpio_direction_input(rx_pins[i]);
        myports[i].rx_gpio = gpio_to_desc(rx_pins[i]);


        // Map the RX Pin to Hardware IRQ_HANDLER

        my_ports[i].rx_irq = gpio_to_irq(rx_pins[i]);
        if (my_ports[i].rx_irq  = gpio_to_irq(rx_pins[i])) {
            printk(KERN_ERR "Soft UART: Failed to map RX GPIO to IRQ\n");
            return my_ports[i].rx_irq;
        }

        // 4. Attach the interrupt handler 
        ret = request_irq(my_ports[i].rx_irq, rx_interrupt_handler, IRQF_TRIGGER_FALLING, "soft_uart_rx_irq",&my_ports[i])
          if (ret ) {
              print(KERN_ERR "Soft UART: Failed to request IRQ");
              return ret;

          }
        printk(KERN_INFO "Soft UART Port %d: TX on GPIO %d, RX on GPIO %d (IRQ %d)\n", 
               i, tx_pins[i], rx_pins[i], my_ports[i].rx_irq);
        my_ports[i].minor_num = i;

        // Initialize the sleeping queue for 'cat'
        init_waitqueue_head(&my_ports[i].read_wait);

        // Create the actual device nodes: /dev/fb0
        device_create(uart_class , NULL , MKDEV(MAJOR(dev_num),i), NULL, "soft_uart%d",i+1);
        
        printk(KERN_INFO "Soft UART: Port %d initialized as /dev/soft_uart%d\n",i,i+1);
    }
    return 0;
}

static void __exit soft_uart_exit(void) {
    int i;
    for (i = 0; i < MAX_PORTS; i++) {
        // Free the interrupt
        free_irq(my_ports[i].rx_irq, &my_ports[i]);
        
        // Free the GPIO pins
        gpio_free(tx_pins[i]); // Note: you'll need tx_pins/rx_pins arrays in scope, 
        gpio_free(rx_pins[i]); // or retrieve the number via desc_to_gpio(my_ports[i].tx_gpio)
        
        device_destroy(uart_class, MKDEV(MAJOR(dev_num), i));
    }

    // 2. Clean up the class and character device
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
