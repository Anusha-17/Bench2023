#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

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
	{ 0x32ab4392, "cdev_init" },
	{ 0x2dcc02c1, "cdev_add" },
	{ 0xeea3c1d8, "__class_create" },
	{ 0x4c0ac327, "device_create" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0x6a2dd7c, "class_destroy" },
	{ 0x37a0cba, "kfree" },
	{ 0xad6d045f, "kmalloc_caches" },
	{ 0x850e6a88, "kmalloc_trace" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x7e38d25d, "device_destroy" },
	{ 0xf7fb9e48, "cdev_del" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x122c3a7e, "_printk" },
	{ 0x453e7dc, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "7CFC14F0353346F08B5C981");
