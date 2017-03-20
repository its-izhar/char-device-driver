/**
* @Author: Izhar Shaikh <izhar>
* @Date:   2017-03-19T22:51:36-04:00
* @Email:  izharits@gmail.com
* @Filename: module.h
 * @Last modified by:   izhar
 * @Last modified time: 2017-03-20T03:07:45-04:00
* @License: MIT
*/



#ifndef __ASP_MYCDEV__
#define __ASP_MYCDEV__

/* Defaul size of each device */
#define		DEFAULT_RAMDISK_SIZE	16*PAGE_SIZE

/* Dynamic Major by default */
#define   DEFAULT_MAJOR         0
#define   DEFAULT_MINOR         0

/* Max number of devices by default */
#define   NUM_DEVICES           3

/* Device struct */
struct asp_mycdev
{
	int devID;						/* device ID */
	char *ramdisk;				/* device */
	size_t ramdiskSize; 	/* device size */
	struct semaphore sem;	/* sync object */
	struct cdev cdev;			/* char device struct */
	bool devReset;				/* flag to indicate that the device is reset */
};

#endif
