/**
* @Author: Izhar Shaikh <izhar>
* @Date:   2017-03-14T14:20:20-04:00
* @Email:  izharits@gmail.com
* @Filename: cdev.c
* @Last modified by:   izhar
* @Last modified time: 2017-03-14T14:52:43-04:00
* @License: MIT
*/



// Simple kernel module boilerplate
#include <linux/module.h>	/* Needed by all modules */
#include <linux/kernel.h>	/* Needed for KERN_INFO */
#include <linux/init.h>		/* Needed for the macros */

static int __init init(void)
{
	printk(KERN_INFO "Hello world!\n");
	return 0;
}
module_init(init);

static void __exit cleanup(void)
{
	printk(KERN_INFO "Goodbye world!\n");
}
module_exit(cleanup);
