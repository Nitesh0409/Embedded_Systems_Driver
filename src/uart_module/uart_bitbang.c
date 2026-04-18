#include <linux/module.h>
#include <linux/init.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

/* Module Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Aaditya");
MODULE_DESCRIPTION("Software UART Bitbang - Timer Test");

/* Timer Variables */
static struct hrtimer my_hrtimer;
static ktime_t timer_interval;
static int tick_count = 0;

/* The function that runs when the timer expires */
static enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
    tick_count++;
    pr_info("UART Bitbang: Timer tick %d\n", tick_count);

    /* For this test, let's stop after 5 ticks so it doesn't run forever */
    if (tick_count >= 5) {
        pr_info("UART Bitbang: Stopping timer.\n");
        return HRTIMER_NORESTART;
    }

    /* Push the timer forward and restart it */
    hrtimer_forward_now(timer, timer_interval);
    return HRTIMER_RESTART;
}

/* Runs when you type 'insmod uart_bitbang.ko' */
static int __init uart_bitbang_init(void)
{
    pr_info("UART Bitbang: Module loading...\n");

    /* Set timer interval to 1 second (1,000,000,000 ns) */
    timer_interval = ktime_set(1, 0); 

    /* Initialize the high-resolution timer */
    hrtimer_init(&my_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    my_hrtimer.function = &timer_callback;

    /* Start the timer */
    hrtimer_start(&my_hrtimer, timer_interval, HRTIMER_MODE_REL);

    return 0;
}

/* Runs when you type 'rmmod uart_bitbang' */
static void __exit uart_bitbang_exit(void)
{
    int cancelled;
    
    /* Safely cancel the timer if it is still running */
    cancelled = hrtimer_cancel(&my_hrtimer);
    
    if (cancelled)
        pr_info("UART Bitbang: Timer was still running, cancelled it.\n");
        
    pr_info("UART Bitbang: Module unloaded.\n");
}

module_init(uart_bitbang_init);
module_exit(uart_bitbang_exit);
