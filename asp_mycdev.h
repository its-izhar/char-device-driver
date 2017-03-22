/**
 * @Author: Izhar Shaikh <izhar>
 * @Date:   2017-03-20T19:24:05-04:00
 * @Email:  izharits@gmail.com
 * @Filename: asp_mycdev.h
 * @Last modified by:   izhar
 * @Last modified time: 2017-03-22T01:27:58-04:00
 * @License: MIT
 */



#ifndef __ASP_MYCDEV__
#define __ASP_MYCDEV__

#include <linux/mutex.h>
#include <linux/device.h>

/* Defaul size of each device - keep it multiple of PAGE_SIZE */
#define  DEFAULT_RAMDISK_SIZE  2*PAGE_SIZE

/* Dynamic Major by default */
#define   DEFAULT_MAJOR         0
#define   DEFAULT_MINOR         0

/* Max number of devices by default */
/* mycdev0 to mycdev3 */
#define   DEFAULT_NUM_DEVICES  3

/* Module name */
#define  MODULE_NAME     "asp_mycdev"
#define  MODULE_CLASS_NAME  "asp_mycdev_class"
#define  MODULE_NODE_NAME   "mycdev"
#define  MAX_NODE_NAME_SIZE  10

/* Device struct */
struct asp_mycdev
{
	int devID; /* device ID */
	char *ramdisk; /* device */
	size_t ramdiskSize; /* device size */
	struct mutex lock; /* mutex for this device */
	struct cdev cdev; /* char device struct */
	struct device *device; /* device node in sysfs */
	bool devReset; /* flag to indicate that the device is reset */
};

/* IOCTLs */
#define ASP_MYCDEV_MAGIC  0x37

/* clear the ramdisk and sets the file position at the beginning */
#define ASP_CLEAR_BUF  _IO(ASP_MYCDEV_MAGIC, 0)

/* Maximum number of IOCTL defs implemented in this driver */
#define ASP_IOCTL_MAXNR  0

#endif /* __ASP_MYCDEV__ */
