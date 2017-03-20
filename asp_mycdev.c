/**
* @Author: Izhar Shaikh <izhar>
* @Date:   2017-03-19T22:00:12-04:00
* @Email:  izharits@gmail.com
* @Filename: module.c
 * @Last modified by:   izhar
 * @Last modified time: 2017-03-20T13:50:55-04:00
* @License: MIT
*/



#include <linux/module.h>   /* Needed by all modules */
#include <linux/kernel.h>   /* Needed for KERN_INFO */
#include <linux/init.h>    /* Needed for the macros */
#include <linux/cdev.h>    /* Needed for cdev struct */
#include <linux/fs.h>     /* Needed for file_operations */
#include <linux/slab.h>    /* Needed for kmalloc, kzalloc etc. */
#include <linux/errno.h>   /* Needed for error checking */
#include <linux/mutex.h>
#include "asp_mycdev.h"    /* Custom header for the drivers */


/* Parameters that can be changed at load time */
static int mycdev_major = DEFAULT_MAJOR;
static int mycdev_minor = DEFAULT_MINOR;
static int max_devices = NUM_DEVICES;
static long ramdisk_size_in_bytes = DEFAULT_RAMDISK_SIZE;

module_param(mycdev_major, int, S_IRUGO);
module_param(mycdev_minor, int, S_IRUGO);
module_param(max_devices,  int, S_IRUGO);
module_param(ramdisk_size_in_bytes, long, S_IRUGO);


/* Other global variables */
static struct asp_mycdev *mycdev_devices = NULL;
static int ramdiskFailDevIndex = -1;

/* Function declarations */
static int mycdev_init_module(void);
static void mycdev_cleanup_module(void);

#if 0
static int asp_mycdev_open(struct inode *, struct file *);
static int asp_mycdev_release(struct inode *, struct file *);
static ssize_t asp_mycdev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t asp_mycdev_write(struct file *, const char __user *, size_t, loff_t *);
#endif

/* Function definitions */
// TODO :: Implement read/write/open/release


/* fileops for asp_mycdev */
static struct file_operations asp_mycdev_fileops = {
	.owner  = THIS_MODULE,
#if 0
	.open   = asp_mycdev_open,
	.release = asp_mycdev_release,
	.read   = asp_mycdev_read,
	.write  = asp_mycdev_write,
#endif
};



/**
 * setup_cdev -
 * @dev: custom device struct for this driver
 * @index: index/offset to add to start of minor
 * Description: Helper function for init to setup cdev struct in asp_mycdev.
 */
static void setup_cdev(struct asp_mycdev *dev, int index)
{
	int error = 0;
	dev_t devNo = 0;

	/* Device Number */
	devNo = MKDEV(mycdev_major, mycdev_minor + index);
	/* Init cdev */
	cdev_init(&dev->cdev, &asp_mycdev_fileops);
	dev->cdev.owner = THIS_MODULE,
	dev->cdev.ops = &asp_mycdev_fileops;
	/* Add the device, NOTE:: This makes the device go live! */
	error = cdev_add(&dev->cdev, devNo, 1);
	/* report error */
	if(error) {
		printk(KERN_WARNING "%s: Error %d adding mycdev%d\n", MODULE_NAME, error, index);
	}
}


/* Init function */
/**
 * mycdev_init_module -
 * Description: Initialization of Module
 * Return: zero if success, errno on perticular error
 */
static int mycdev_init_module(void)
{
	dev_t devNum = 0;
	bool ramdiskAllocFailed = false;
	int i = 0, retval = 0;

	printk(KERN_INFO "%s: Initializing Module!\n", MODULE_NAME);

	/* Allocate major and range of minor numbers to work with the driver dynamically
	 unless otherwise specified at load time */
	if(mycdev_major || mycdev_minor) {
		devNum = MKDEV(mycdev_major, mycdev_minor);
		retval = register_chrdev_region(devNum, max_devices, "mycdev");
	}
	else {
		retval = alloc_chrdev_region(&devNum, mycdev_minor, max_devices, "mycdev");
		mycdev_major = MAJOR(devNum);
	}
	if(retval < 0){
		printk(KERN_WARNING "%s: Unable to allocate major %d\n", MODULE_NAME, mycdev_major);
		return retval;
	}
	printk(KERN_DEBUG "%s: Requested Devices - %d, Major :- %d, Minor - %d\n",\
										 MODULE_NAME, max_devices, mycdev_major, mycdev_minor);

	/* Allocate and setup the devices here */
	mycdev_devices = kzalloc(max_devices * sizeof(struct asp_mycdev), GFP_KERNEL);
	if(mycdev_devices == NULL){
		retval = -ENOMEM;
		goto FAIL;
	}

	/* Setup the devices */
	for(i = 0; i < max_devices; i++)
	{
		/* Device Reset flag */
		mycdev_devices[i].devReset = true;
		/* Device number */
		mycdev_devices[i].devID = i;
		/* Initializing Mutex */
		mutex_init(&mycdev_devices[i].lock);

		/* Initializing ramdisk */
		mycdev_devices[i].ramdisk = kzalloc((size_t) ramdisk_size_in_bytes, GFP_KERNEL);
		if(mycdev_devices[i].ramdisk == NULL){
			/* mark that we failed to allocate current device memory,
			we will clean up previously allocated devices in cleanup module */
			ramdiskFailDevIndex = i;
			ramdiskAllocFailed = true;
			break;	/* exit for */
		}
		mycdev_devices[i].ramdiskSize = ramdisk_size_in_bytes;

		/* Setup cdev struct here */
		setup_cdev(&mycdev_devices[i], i);
	}
	/* cleanup if we failed to allocate device memory */
	if(ramdiskAllocFailed == true){
		printk(KERN_WARNING "%s: Unable to allocate device memory %d\n",\
		 										MODULE_NAME, ramdiskFailDevIndex);
		retval = -ENOMEM;
		goto FAIL;
	}

	printk(KERN_INFO "%s: Initialization Complete!\n", MODULE_NAME);

	return 0;

FAIL:
	mycdev_cleanup_module();
	return retval;
}
module_init(mycdev_init_module);



/* Exit function */
/**
 * mycdev_cleanup_module -
 * Description: Cleanup routine for this module
 */
static void mycdev_cleanup_module(void)
{
	int i = 0;
	int lastDevice = -1;

	printk(KERN_INFO "%s: Cleaning Up Module!\n", MODULE_NAME);

	/* check if we have failed during device init phase,
	if we have, we will only free up previously allocated device,
	i.e. devices until last index, leaving current index */
	if(ramdiskFailDevIndex == 0) {	/* None allocated */
		lastDevice = -1;
	}
	else if(ramdiskFailDevIndex > 0 && ramdiskFailDevIndex < max_devices) {	/* A few */
		lastDevice = ramdiskFailDevIndex;
	}
	else if(ramdiskFailDevIndex == -1) {	/* All allocated */
		lastDevice = max_devices;
	}

	/* Cleanup devices */
	if(mycdev_devices != NULL)
	{
		/* free up ramdisk and cdev only for allocated devices */
		for(i = 0; i < lastDevice; i++)
		{
			if(mycdev_devices[i].ramdisk != NULL)
			{
				kfree(mycdev_devices[i].ramdisk);
				mycdev_devices[i].ramdisk = NULL;
			}
			cdev_del(&mycdev_devices[i].cdev);
		}
		/* free up device array */
		kfree(mycdev_devices);
		mycdev_devices = NULL;
	}
	printk(KERN_DEBUG "%s: Freed up %d devices/ramdisks.\n", MODULE_NAME, lastDevice);

	/* Cleaning up the chrdev_region,
	this is never called if the registration failes */
	unregister_chrdev_region(MKDEV(mycdev_major, mycdev_minor), max_devices);

	printk(KERN_INFO "%s: Cleanup Done!\n", MODULE_NAME);
}
module_exit(mycdev_cleanup_module);

/* Driver Info */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Izhar Shaikh");
MODULE_DESCRIPTION("ASP: Assignment 5 - Simple Char Driver");
