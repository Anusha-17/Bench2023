#include<linux/kernel.h>
#include<linux/init.h>
#include<linux/kernel.h>
#include<linux/kdev_t.h>
#include<linux/fs.h>
#include<linux/err.h>
#include<linux/device.h>

dev_t dev=0;
static struct class *dev_class;

static int __init hello_world_init(void)
{
	if((alloc_chrdev_region(&dev, 0, 1, "my_dev")) < 0)
	{
		pr_err("Cannot allocate major number for device\n");
		return -1;
	}
	pr_info("Major = %d Minor = %d \n", MAJOR(dev), MINOR(dev));
	
	dev_class = class_create(THIS_MODULE,"my_class"); //creating struct class
	if(IS_ERR(dev_class))
	{
		pr_err("Cannot create the struct class for device\n");
		goto r_class;
	}
	if(IS_ERR(device_create(dev_class,NULL,dev,NULL,"my_device"))) //creating device
	{
		pr_err("Cannot create the Device\n");
		goto r_device;
	}
	pr_info("Kernel Module Inserted Successfully...\n");
	return 0;
	
r_device:
	class_destroy(dev_class);
r_class:
	unregister_chrdev_region(dev,1);
	return -1;
}

static void __exit hello_world_exit(void)
{
	device_destroy(dev_class,dev);
	class_destroy(dev_class);
	unregister_chrdev_region(dev, 1);
	pr_info("Kernel Module Removed Successfully...\n");
}

module_init(hello_world_init);
module_exit(hello_world_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Anusha");
MODULE_DESCRIPTION("Automatically Creating a Device file");
MODULE_VERSION("1.2");

