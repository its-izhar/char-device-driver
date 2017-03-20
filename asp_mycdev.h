/**
* @Author: Izhar Shaikh <izhar>
* @Date:   2017-03-19T22:51:36-04:00
* @Email:  izharits@gmail.com
* @Filename: module.h
 * @Last modified by:   izhar
 * @Last modified time: 2017-03-20T13:41:43-04:00
* @License: MIT
*/



#ifndef __ASP_MYCDEV__
#define __ASP_MYCDEV__

#include <linux/mutex.h>

/* Defaul size of each device */
#define		DEFAULT_RAMDISK_SIZE	16*PAGE_SIZE

/* Dynamic Major by default */
#define   DEFAULT_MAJOR         0
#define   DEFAULT_MINOR         0

/* Max number of devices by default */
/* mycdev0 to mycdev3 */
#define   NUM_DEVICES           3

/* Module name */
#define		MODULE_NAME						"asp_mycdev"

/* Device struct */
struct asp_mycdev
{
	int devID;						/* device ID */
	char *ramdisk;				/* device */
	size_t ramdiskSize; 	/* device size */
	struct mutex lock;		/* mutex for this device */
	struct cdev cdev;			/* char device struct */
	bool devReset;				/* flag to indicate that the device is reset */
};

#endif
