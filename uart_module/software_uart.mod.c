#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x27bbf221, "disable_irq_nosync" },
	{ 0xd955afa6, "hrtimer_start_range_ns" },
	{ 0x331450f1, "gpiod_set_value" },
	{ 0xf07464e4, "hrtimer_forward" },
	{ 0x4a666d56, "hrtimer_cancel" },
	{ 0xc1514a3b, "free_irq" },
	{ 0x6b891a2, "desc_to_gpio" },
	{ 0xfe990052, "gpio_free" },
	{ 0x522bfddd, "device_destroy" },
	{ 0x500aa300, "class_destroy" },
	{ 0x235cf88, "cdev_del" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x122c3a7e, "_printk" },
	{ 0xdcb764ad, "memset" },
	{ 0x12a4e128, "__arch_copy_from_user" },
	{ 0xe2964344, "__wake_up" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x56c8c364, "gpiod_get_value" },
	{ 0xfcec0987, "enable_irq" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x1b6e3a4, "cdev_init" },
	{ 0xb1e6e170, "cdev_add" },
	{ 0xd868b6c, "class_create" },
	{ 0x48879f20, "hrtimer_init" },
	{ 0x47229b5c, "gpio_request" },
	{ 0x2e5879f3, "gpio_to_desc" },
	{ 0x31801c1, "gpiod_direction_output_raw" },
	{ 0xe9771505, "gpiod_direction_input" },
	{ 0x65912c4d, "gpiod_to_irq" },
	{ 0x92d5838e, "request_threaded_irq" },
	{ 0xd9a5ea54, "__init_waitqueue_head" },
	{ 0xd8f9c29e, "device_create" },
	{ 0x6cbbfc54, "__arch_copy_to_user" },
	{ 0xfe487975, "init_wait_entry" },
	{ 0x1000e51, "schedule" },
	{ 0x8c26d495, "prepare_to_wait_event" },
	{ 0x92540fbf, "finish_wait" },
	{ 0xb75fe670, "module_layout" },
};

MODULE_INFO(depends, "");

