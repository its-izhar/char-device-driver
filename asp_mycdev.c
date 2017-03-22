/**
 * @Author: Izhar Shaikh <izhar>
 * @Date:   2017-03-20T19:44:34-04:00
 * @Email:  izharits@gmail.com
 * @Filename: asp_mycdev.c
 * @Last modified by:   izhar
 * @Last modified time: 2017-03-21T22:07:43-04:00
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
loff_t asp_mycdev_lseek(struct file *, loff_t, int);
long asp_mycdev_ioctl(struct file *, unsigned int, unsigned long);

/* Function definitions */
/* open function */
/**
 * asp_mycdev_open -
 * @i_ptr: pointer to current inode being pointed after open sys call
 * @filp:	pointer to current file descriptor struct after open sys call
 * Description:
 		Extracts the custom device struct from current cdev, and stores it in file's
		private_data field
 * Return: 0, Always succeeds
 */
static int asp_mycdev_open(struct inode *i_ptr, struct file *filp)
{
	struct asp_mycdev *mycdev = NULL;

	/* Get the struct of current device */
	mycdev = container_of(i_ptr->i_cdev, struct asp_mycdev, cdev);

	/* Make device ready for future use */
	mycdev->devReset = false;		/* this is needed so read doesn't freak out */
	filp->private_data = mycdev;		/* for later use by other functions */

	printk(KERN_INFO "%s: device %s%d opened [Major: %d, Minor: %d]\n",\
	 	MODULE_NAME, "/dev/"MODULE_NODE_NAME, mycdev->devID, imajor(i_ptr), iminor(i_ptr));
	return 0;
}


/* release function */
/**
 * asp_mycdev_release -
 * @i_ptr: current inode being referred to`
 * @filp: file descriptor pointer
 * Description: Releases the device
 * Return: 0, Always succeeds
 */
static int asp_mycdev_release(struct inode *i_ptr, struct file *filp)
{
	struct asp_mycdev *mycdev = filp->private_data;

	printk(KERN_INFO "%s: device %s%d closed\n",\
	 	MODULE_NAME, "/dev/"MODULE_NODE_NAME, mycdev->devID);
	return 0;
}


/* read from device */
/**
 * asp_mycdev_read -
 * @filp: file pointer
 * @buf: buffer handle provided from userspace
 * @count: bytes requested to read and store in buf
 * @f_offset: current position in the file
 * Description:
 		Reads requested number of bytes from device and updates the current position
		in the file
 * Return: Number of bytes read from the device
 */
static ssize_t asp_mycdev_read(struct file *filp, char __user *buf, size_t count,\
	 loff_t *f_offset)
{
	struct asp_mycdev *mycdev = filp->private_data;
	ssize_t retval = 0;

	if(mutex_lock_interruptible(&mycdev->lock))		/* ENTER Critical Section */
		return -ERESTARTSYS;
	if(mycdev->devReset == true)				/* device has been reset by IOCTL */
		goto EXIT;
	if(*f_offset > mycdev->ramdiskSize)			/* already done */
		goto EXIT;
	if((count + *f_offset) > mycdev->ramdiskSize) { /* read beyond our device size */
		printk(KERN_WARNING "%s: device %s%d: Attempt to READ beyond the device size!\n",\
			MODULE_NAME, "/dev/"MODULE_NODE_NAME, mycdev->devID);
			/* read only upte the device size */
			count = mycdev->ramdiskSize - *f_offset;
	}

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
/**
 * asp_mycdev_write
 * @filp: file pointer
 * @buf: buffer handle provided from userspace
 * @count: bytes requested to write from buffer
 * @f_offset: current position in the file
 * Description:
 		Writes the requested number of bytes to the device and updates the file position
		in the device
 * Return: Number of bytes written to the device
 */
static ssize_t asp_mycdev_write(struct file *filp, const char __user *buf, \
	size_t count, loff_t *f_offset)
{
	struct asp_mycdev *mycdev = filp->private_data;
	ssize_t retval = -ENOMEM;

	if(mutex_lock_interruptible(&mycdev->lock))		/* ENTER Critical Section */
		return -ERESTARTSYS;
	if((count + *f_offset) > mycdev->ramdiskSize) { /* write beyond our device size */
		printk(KERN_WARNING "%s: device %s%d: Attempt to WRITE beyond the device size! Returning!\n",\
			MODULE_NAME, "/dev/"MODULE_NODE_NAME, mycdev->devID);
		goto EXIT;
	}

	/* copy to user and update the offset in the device */
	mycdev->devReset = false;
	retval = count - copy_from_user((mycdev->  ramdisk + *f_offset), buf, count);
	*f_offset += retval;

	printk(KERN_DEBUG "%s: device %s%d: bytes written: %d, current position: %d\n",\
		MODULE_NAME, "/dev/"MODULE_NODE_NAME, mycdev->devID, (int)retval, (int)*f_offset);

EXIT:
	mutex_unlock(&mycdev->lock);			/* EXIT Critical Section */
	return retval;
}


/* set the ramdisk offset to desired offset in the device */
/**
 * asp_mycdev_lseek -
 * @filp: file pointer
 * @f_offset: requested offset to be set the file
 * @action: SEEK_SET/ SEEK_CUR/ SEEK_END
 * Description:
 		Set the current position in ramdisk to desired offset, based on action:
		SEEK_SET: set to requested offset
		SEEK_CUR: set to current offset + requested offset
		SEEK_END: set to the requested offset from the end of file

		This function also resizes the ramdisk in the device if the requested offset
		is beyond the current file ramdisk size, and fills the extra region with zeros
 * Return:
 */
loff_t asp_mycdev_lseek(struct file *filp, loff_t f_offset, int action)
{
	loff_t new_offset;
	struct asp_mycdev *mycdev = filp->private_data;

	/* ENTER Critical Section */
	if(mutex_lock_interruptible(&mycdev->lock))
		return -ERESTARTSYS;

	switch (action)
	{
		case SEEK_SET:
			new_offset = f_offset;
			break;

		case SEEK_CUR:
			new_offset = filp->f_pos + f_offset;
			break;

		case SEEK_END:
			new_offset = mycdev->ramdiskSize + f_offset;
			break;

		default:
			new_offset = -EINVAL;
			goto EXIT;
	}
	/* validity checks (lower boundary) */
	new_offset = (new_offset < 0)? 0: new_offset;

	printk(KERN_DEBUG "%s: device %s%d: Current offset: %ld, Requested offset: %ld\n",\
	MODULE_NAME, "/dev/"MODULE_NODE_NAME, mycdev->devID, (long) filp->f_pos, (long) new_offset);

	/* if the new_offset is beyond the current size of ramdisk,
	reallocate ramdisk to hold double the current size
	and fill the remaining region with all zeros */
	if(new_offset > mycdev->ramdiskSize)
	{
		char *new_ramdisk = NULL;
		int pages = -1;
		size_t old_ramdiskSize = -1;
		size_t new_ramdiskSize = -1;

		/* find the new ramdisk size which is multiple of PAGE_SIZE */
		pages = new_offset / PAGE_SIZE;		// Assert (pages >= 1)
		pages = (new_offset % PAGE_SIZE > 0)? pages+1 : pages;
		new_ramdiskSize = pages * PAGE_SIZE;

		/* reallocate ramdisk */
		new_ramdisk = krealloc(mycdev->ramdisk, new_ramdiskSize, GFP_KERNEL);
		if(new_ramdisk != NULL)
		{
			/* save old ramdiskSize, we will need it to update the expanded memory */
			old_ramdiskSize = mycdev->ramdiskSize;
			/* realloc succeeded, zero out the extra memory */
			mycdev->ramdisk = new_ramdisk;
			mycdev->ramdiskSize = new_ramdiskSize;
			memset(mycdev->ramdisk + old_ramdiskSize, 0, new_ramdiskSize - old_ramdiskSize);

			printk(KERN_DEBUG "%s: device %s%d: Ramdisk resized! "
				"old_ramdiskSize: %d, new_ramdiskSize: %d, zerod out memory: %d\n",\
				MODULE_NAME, "/dev/"MODULE_NODE_NAME, mycdev->devID, \
				(int) old_ramdiskSize, (int) new_ramdiskSize, (int) (new_ramdiskSize - old_ramdiskSize));
		}
		else {
			/* realloc failed, old ramdisk handle is still valid */
			printk(KERN_DEBUG "%s: device %s%d: Failed to reallocate ramdisk!\n",\
				MODULE_NAME, "/dev/"MODULE_NODE_NAME, mycdev->devID);

			new_offset = -ENOMEM;
			goto EXIT;
		}
	}
	/* update the current seek */
	filp->f_pos = new_offset;

	printk(KERN_DEBUG "%s: device %s%d: Seeking to position: %ld\n",\
		MODULE_NAME, "/dev/"MODULE_NODE_NAME, mycdev->devID, (long) new_offset);

EXIT:
	mutex_unlock(&mycdev->lock);
	return new_offset;
}


/* IOCTL calls */
long asp_mycdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long retval = -1;
	struct asp_mycdev *mycdev = NULL;

	/* Extracts type and number bitfields;
	don't decode wrong commands; return -ENOTTY (Inappropriate IOCTL)  */
	if(_IOC_TYPE(cmd) != ASP_MYCDEV_MAGIC)
		return -ENOTTY;
	if(_IOC_NR(cmd) > ASP_IOCTL_MAXNR)
		return -ENOTTY;

	/* If everything is fine, extract the command and perform action */
	mycdev = filp->private_data;
	switch (cmd)
	{
		/* clear the ramdisk & seek to start of the file */
		case ASP_CLEAR_BUF:
			memset(mycdev->ramdisk, 0, mycdev->ramdiskSize);
			filp->f_pos = 0;
			mycdev->devReset = true;
			retval = 1;
			break;

		/* the control is unlikely to come here after MAXNR check above */
		default:
			return -ENOTTY;
	}

	/* Just to debug */
	if(retval == 1){
		printk(KERN_DEBUG "%s: device %s%d: Successful Reset!\n",\
			MODULE_NAME, "/dev/"MODULE_NODE_NAME, mycdev->devID);
	}
	return retval;
}


/* fileops for asp_mycdev */
static struct file_operations asp_mycdev_fileops = {
	.owner  = THIS_MODULE,
	.open   = asp_mycdev_open,
	.read   = asp_mycdev_read,
	.llseek = asp_mycdev_lseek,
	.write  = asp_mycdev_write,
	.release = asp_mycdev_release,
	.unlocked_ioctl = asp_mycdev_ioctl,
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
	if(IS_ERR_OR_NULL(asp_mycdev_class)){
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
		if(IS_ERR_OR_NULL(mycdev_devices[i].device))
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

		printk(KERN_DEBUG "%s: Freed up %d devices/ramdisks.\n",\
			MODULE_NAME, lastSuccessfulRamdisk + 1);
		printk(KERN_DEBUG "%s: Freed up %d devices/nodes.\n",\
			MODULE_NAME, lastSuccessfulNode + 1);
		printk(KERN_DEBUG "%s: Freed up %d devices/cdevs.\n",\
			MODULE_NAME, lastSuccessfulCdev + 1);
	}

	/* Clean up device class */
	if(!IS_ERR_OR_NULL(asp_mycdev_class)){
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
