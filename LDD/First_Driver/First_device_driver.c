#include<linux/module.h>

MODULE_LICENSE("GPL");
MODULE_LICENSE("GPL V2");
MODULE_LICENSE("Dual BSD/GPL");

MODULE_AUTHOR("Anusha");
MODULE_DESCRIPTION("A sample driver");
MODULE_VERSION("1:0.0");

static int __init hello_world_init(void)
{
	printk(KERN_INFO "Hello.. This is my first driver");
	return 0;
}
module_init(hello_world_init);

void __exit hello_world_exit(void)
{
	printk(KERN_INFO "Exiting my driver");
}
module_exit(hello_world_exit);
