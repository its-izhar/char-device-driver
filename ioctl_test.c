/**
 * @Author: Izhar Shaikh <izhar>
 * @Date:   2017-03-21T21:21:09-04:00
 * @Email:  izharits@gmail.com
 * @Filename: ioctl_test.c
 * @Last modified by:   izhar
 * @Last modified time: 2017-03-24T12:46:37-04:00
 * @License: MIT
 */



/*
   IOCTL Test
   @*/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#define ASP_CLEAR_BUF   _IO(0x37, 0)

int main(int argc, char **argv)
{
  int length, fd1, fd2, rc;
  char *nodename = "/dev/mycdrv0";
  char message[] = " *** IOCTL Test ***\n";
  char r_message[sizeof(message)] = { 0 };

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

  rc = read(fd2, r_message, length);
  printf("return code from read  = %d on %d, message=%s\n", rc, fd2,
         r_message);

  /* Resetting the device now */
  printf("Resetting the device now ...\n");
  if(ioctl(fd2, ASP_CLEAR_BUF) == 1) {
          printf("ASP_CLEAR_BUF has succesfully reset %s\n", nodename);
  }
  else {
          printf("IOCTL: ASP_CLEAR_BUF failed!\n");
  }

  memset(r_message, 0, sizeof(r_message));
  rc = read(fd2, r_message, length);
  printf("Now, this read should not return anything since the buffer is cleared: \n");
  printf("return code from read  = %d on %d, message=%s\n", rc, fd2,
         r_message);

  close(fd1);
  close(fd2);

  exit(0);
}
