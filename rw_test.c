/**
 * @Author: Izhar Shaikh <izhar>
 * @Date:   2017-03-21T14:54:08-04:00
 * @Email:  izharits@gmail.com
 * @Filename: ctest.c
 * @Last modified by:   izhar
 * @Last modified time: 2017-03-21T15:30:14-04:00
 * @License: MIT
 */



/*
   Basic read/write program
 @*/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	int length, fd1, fd2, rc;
	char *nodename = "/dev/mycdrv0";
	char message[] = " *** TESTING CHAR/DRIVER ***\n";

	length = sizeof(message);

	if (argc == 2) {
		nodename = argv[1];
	}
	else {
		printf("USAGE:\n\t %s <device-node-name>\n", argv[0]);
		return 0;
	}

	fd1 = open(nodename, O_RDWR);
	printf(" opened file descriptor first time  = %d\n", fd1);
	fd2 = open(nodename, O_RDWR);
	printf(" opened file descriptor second time  = %d\n", fd2);

	rc = write(fd1, message, length);
	printf("return code from write = %d on %d, message=%s\n", rc, fd1,
	       message);

	memset(message, 0, length);

	rc = read(fd2, message, length);
	printf("return code from read  = %d on %d, message=%s\n", rc, fd2,
	       message);

	close(fd1);
	close(fd2);
	
	exit(0);
}
