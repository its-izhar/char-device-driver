/**
 * @Author: Izhar Shaikh <izhar>
 * @Date:   2017-03-21T14:54:14-04:00
 * @Email:  izharits@gmail.com
 * @Filename: ltest.c
 * @Last modified by:   izhar
 * @Last modified time: 2017-03-21T15:20:55-04:00
 * @License: MIT
 */



/*
 * Keeping track of file position. (Testing application)
 @*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char *argv[])
{
	int length = 20, position = 0, fd, rc;
	char *message, *nodename = "/dev/mycdrv0";

	if(argc == 4) {
		nodename = argv[1];
		position = atoi(argv[2]);
		length = atoi(argv[3]);
	}
	else {
		printf("USAGE:\n\t %s <device-node-name> <position-to-seek> <message-length>\n", argv[0]);
		return 0;
	}

	/* set up the message */
	message = malloc(length);
	memset(message, 'x', length/2);
	memset(message + (length/2), 'y', length/2);
	message[length - 1] = '\0';	/* make sure it is null terminated */

	/* open the device node */
	fd = open(nodename, O_RDWR);
	printf(" I opened the device node, file descriptor = %d\n", fd);

	/* seek to position */
	rc = lseek(fd, position, SEEK_SET);
	printf("return code from lseek = %d\n", rc);

	/* write to the device node twice */
	rc = write(fd, message, length);
	printf("return code from write = %d\n", rc);
	//rc = write(fd, message, length);
	//printf("return code from write = %d\n", rc);

	/* reset the message to null */
	memset(message, 0, length);

	/* seek to position */
	rc = lseek(fd, position, SEEK_SET);
	printf("return code from lseek = %d\n", rc);

	/* read from the device node */

	rc = read(fd, message, length);
	printf("return code from read = %d\n", rc);
	printf("the message read [len: %d]: %s\n", rc, message);

	close(fd);
	exit(0);

}
