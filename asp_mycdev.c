/**
 * @Author: Izhar Shaikh <izhar>
 * @Date:   2017-03-20T19:44:34-04:00
 * @Email:  izharits@gmail.com
 * @Filename: asp_mycdev.c
 * @Last modified by:   izhar
 * @Last modified time: 2017-03-20T22:46:08-04:00
 * @License: MIT
 */



#include <linux/module.h>   /* Needed by all modules */
#include <linux/kernel.h>   /* Needed for KERN_INFO */
#include <linux/init.h>    /* Needed for the macros */
#include <linux/cdev.h>    /* Needed for cdev struct */
#include <linux/fs.h>     /* Needed for file_operations */
#include <linux/slab.h>    /* Needed for kmalloc, kzalloc etc. */
#include <linux/errno.h>   /* Needed for error checking */
#include <linux/mutex.h>	/* Sync primitives */
#include <linux/device.h>	/* device class */
#include <asm/uaccess.h>	/* copy_*_user */

#include "asp_mycdev.h"    /* Custom header for the drivers */

/* Parameters that can be changed at load time */
static int mycdev_major = DEFAULT_MAJOR;
static int mycdev_minor = DEFAULT_MINOR;
static int max_devices = DEFAULT_NUM_DEVICES;
static long ramdisk_size_in_bytes = DEFAULT_RAMDISK_SIZE;

module_param(mycdev_major, int, S_IRUGO);
module_param(mycdev_minor, int, S_IRUGO);
module_param(max_devices,  int, S_IRUGO);
module_param(ramdisk_size_in_bytes, long, S_IRUGO);


/* Other global variables */
static struct class *asp_mycdev_class = NULL;
static struct asp_mycdev *mycdev_devices = NULL;
static int lastSuccessfulRamdisk = -1;
static int lastSuccessfulCdev = -1;
static int lastSuccessfulNode = -1;

/* Function declarations */
static int mycdev_init_module(void);
static void mycdev_cleanup_module(void);
static int asp_mycdev_open(struct inode *, struct file *);
static int asp_mycdev_release(struct inode *, struct file *);
static ssize_t asp_mycdev_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t asp_mycdev_write(struct file *, const char __user *, size_t, loff_t *);


/* Function definitions */
/* open function */
static int asp_mycdev_open(struct inode *i_ptr, struct file *filp)
{
	struct asp_mycdev *mycdev = NULL;

	/* Get the struct of current device */
	mycdev = container_of(i_ptr->i_cdev, struct asp_mycdev, cdev);

	/* Make device ready for future use */
	mycdev->devReset = false;
	filp->private_data = mycdev;		/* for later use by other functions */

	printk(KERN_INFO "%s: device %s%d opened [Major: %d, Minor: %d]\n",\
	 	MODULE_NAME, "/dev/"MODULE_NODE_NAME, mycdev->devID, imajor(i_ptr), iminor(i_ptr));
	return 0;
}


/* release function */
static int asp_mycdev_release(struct inode *i_ptr, struct file *filp)
{
	struct asp_mycdev *mycdev = filp->private_data;

	printk(KERN_INFO "%s: device %s%d closed\n",\
	 	MODULE_NAME, "/dev/"MODULE_NODE_NAME, mycdev->devID);
	return 0;
}


/* read from device */
static ssize_t asp_mycdev_read(struct file *filp, char __user *buf, size_t count,\
	 loff_t *f_offset)
{
	struct asp_mycdev *mycdev = filp->private_data;
	ssize_t retval = 0;

	if(mutex_lock_interruptible(&mycdev->lock))		/* ENTER Critical Section */
		return -ERESTARTSYS;
	if(*f_offset > mycdev->ramdiskSize)			/* already done */
		goto EXIT;
	if((count + *f_offset) > mycdev->ramdiskSize)		/* read beyond our device size */
		retval = mycdev->ramdiskSize - *f_offset;

	/* copy to user and update the offset in the device */
	retval = count - copy_to_user(buf, (mycdev->ramdisk + *f_offset), count);
	*f_offset += retval;

	printk(KERN_DEBUG "%s: device %s%d: bytes read: %d, current position: %d\n",\
		MODULE_NAME, "/dev/"MODULE_NODE_NAME, mycdev->devID, (int)retval, (int)*f_offset);

EXIT:
	mutex_unlock(&mycdev->lock);			/* EXIT Critical Section */
	return retval;
}


/* write to device */
static ssize_t asp_mycdev_write(struct file *filp, const char __user *buf, \
	size_t count, loff_t *f_offset)
{
	struct asp_mycdev *mycdev = filp->private_data;
	ssize_t retval = -ENOMEM;

	if(mutex_lock_interruptible(&mycdev->lock))		/* ENTER Critical Section */
		return -ERESTARTSYS;
	if((count + *f_offset) > mycdev->ramdiskSize)		/* write beyond our device size */
		goto EXIT;

	/* copy to user and update the offset in the device */
	retval = count - copy_from_user((mycdev->  ramdisk + *f_offset), buf, count);
	*f_offset += retval;

	printk(KERN_DEBUG "%s: device %s%d: bytes written: %d, current position: %d\n",\
		MODULE_NAME, "/dev/"MODULE_NODE_NAME, mycdev->devID, (int)retval, (int)*f_offset);

EXIT:
	mutex_unlock(&mycdev->lock);			/* EXIT Critical Section */
	return retval;
}


/* fileops for asp_mycdev */
static struct file_operations asp_mycdev_fileops = {
	.owner  = THIS_MODULE,
	.open   = asp_mycdev_open,
	.read   = asp_mycdev_read,
	.write  = asp_mycdev_write,
	.release = asp_mycdev_release,
};


/**
 * setup_cdev -
 * @dev: custom device struct for this driver
 * @index: index/offset to add to start of minor
 * Description: Helper function for init to setup cdev struct in asp_mycdev.
 */
static int setup_cdev(struct asp_mycdev *dev, int index)
{
	int error = 0;
	int retval = 0;
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
		retval = -1;
	}
	return retval;
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
	bool cdevSetupFailed = false;
	bool nodeSetupFailed = false;
	int i = 0, retval = 0;

	printk(KERN_INFO "%s: Initializing Module!\n", MODULE_NAME);

	/* Allocate major and range of minor numbers to work with the driver dynamically
	 unless otherwise specified at load time */
	if(mycdev_major || mycdev_minor) {
		devNum = MKDEV(mycdev_major, mycdev_minor);
		retval = register_chrdev_region(devNum, max_devices, MODULE_NODE_NAME);
	}
	else {
		retval = alloc_chrdev_region(&devNum, mycdev_minor, max_devices, MODULE_NODE_NAME);
		mycdev_major = MAJOR(devNum);
	}
	if(retval < 0){
		printk(KERN_WARNING "%s: Unable to allocate major %d\n", MODULE_NAME, mycdev_major);
		return retval;
	}
	printk(KERN_DEBUG "%s: Requested Devices - %d, Major :- %d, Minor - %d\n",\
		MODULE_NAME, max_devices, mycdev_major, mycdev_minor);

	/* Setup the device class, needed to create device nodes in sysfs */
	asp_mycdev_class = class_create(THIS_MODULE, MODULE_CLASS_NAME);
	if(IS_ERR(asp_mycdev_class)){
		printk(KERN_WARNING "%s: Failed to Init Device Class %s\n",\
			MODULE_NAME, MODULE_CLASS_NAME);
		retval = -1;
		goto FAIL;
	}
	printk(KERN_INFO "%s: Created device class: %s\n", MODULE_NAME, MODULE_CLASS_NAME);

	/* Allocate and setup the devices here */
	mycdev_devices = kzalloc(max_devices * sizeof(struct asp_mycdev), GFP_KERNEL);
	if(mycdev_devices == NULL){
		retval = -ENOMEM;
		goto FAIL;
	}

	/* Setup the devices */
	for(i = 0; i < max_devices; i++)
	{
		char nodeName[MAX_NODE_NAME_SIZE] = { 0 };
		int cdevStatus = 0;

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
			printk(KERN_WARNING "%s: Failed to allocate ramdisk for device %d\n", MODULE_NAME, i);
			ramdiskAllocFailed = true;
			break;	/* exit for */
		}
		lastSuccessfulRamdisk = i;
		mycdev_devices[i].ramdiskSize = ramdisk_size_in_bytes;

		/* Create device node here */
		snprintf(nodeName, sizeof(nodeName), MODULE_NODE_NAME"%d", i);

		mycdev_devices[i].device = device_create(asp_mycdev_class, NULL,\
			MKDEV(mycdev_major, mycdev_minor + i), NULL, nodeName);
		if(IS_ERR(mycdev_devices[i].device))
		{
			/* mark that we failed to create and register current device node with sysfs,
			we will clean up previously device nodes in cleanup module */
			printk(KERN_WARNING "%s: Failed to Create Device Node %s\n", MODULE_NAME, nodeName);
			nodeSetupFailed = true;
			break;
		}
		lastSuccessfulNode = i;

		/* Setup cdev struct here */
		cdevStatus = setup_cdev(&mycdev_devices[i], i);
		if(cdevStatus < 0){
			/* mark that we failed to allocate current cdev,
			we will clean up previously allocated cdevs in cleanup module */
			printk(KERN_WARNING "%s: Failed to setup cdev for device %d\n", MODULE_NAME, i);
			cdevSetupFailed = true;
			break;
		}
		lastSuccessfulCdev = i;
	}
	/* cleanup if we failed to allocate device memory */
	if(ramdiskAllocFailed || nodeSetupFailed || cdevSetupFailed)
	{
		retval = -ENOMEM;
		goto FAIL;
	}

	printk(KERN_INFO "%s: Initialization Complete!\n", MODULE_NAME);
	printk(KERN_INFO "%s: lastSuccessfulRamdisk: %d, lastSuccessfulNode: %d, lastSuccessfulCdev: %d\n",\
	 MODULE_NAME, lastSuccessfulRamdisk, lastSuccessfulNode, lastSuccessfulCdev);

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

	printk(KERN_INFO "%s: Cleaning Up Module!\n", MODULE_NAME);

	/* we will free lastSuccessful constructs here */
	/* Cleanup devices */
	if(mycdev_devices != NULL)
	{
		/* ramdisk */
		for(i = 0; i <= lastSuccessfulRamdisk; i++)
		{
			if(mycdev_devices[i].ramdisk != NULL)
			{
				kfree(mycdev_devices[i].ramdisk);
				mycdev_devices[i].ramdisk = NULL;
			}
		}
		/* cdev */
		for(i = 0; i <= lastSuccessfulCdev; i++)
		{
			cdev_del(&mycdev_devices[i].cdev);
		}
		/* device nodes */
		for(i = 0; i <= lastSuccessfulNode; i++)
		{
			device_destroy(asp_mycdev_class, MKDEV(mycdev_major, mycdev_minor + i));
		}
		/* free up device array */
		kfree(mycdev_devices);
		mycdev_devices = NULL;
		printk(KERN_DEBUG "%s: Freed up %d devices/ramdisks.\n", MODULE_NAME, lastSuccessfulRamdisk+1);
		printk(KERN_DEBUG "%s: Freed up %d devices/nodes.\n", MODULE_NAME, lastSuccessfulNode+1);
		printk(KERN_DEBUG "%s: Freed up %d devices/cdevs.\n", MODULE_NAME, lastSuccessfulCdev+1);
	}

	/* Clean up device class */
	if(!IS_ERR(asp_mycdev_class)){
		class_destroy(asp_mycdev_class);
		asp_mycdev_class = NULL;
		printk(KERN_DEBUG "%s: Freed up %s device class.\n", MODULE_NAME, MODULE_CLASS_NAME);
	}

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
