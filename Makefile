# @Author: Izhar Shaikh <izhar>
# @Date:   2017-03-14T14:17:32-04:00
# @Email:  izharits@gmail.com
# @Filename: Makefile
# @Last modified by:   izhar
# @Last modified time: 2017-03-24T12:52:23-04:00
# @License: MIT



obj-m := asp_mycdev.o

all: clean
	make -C /usr/src/linux-headers-$(shell uname -r) M=$(PWD) modules
	gcc -Wall -Werror -O2 -o rw_test rw_test.c
	gcc -Wall -Werror -O2 -o lseek_test lseek_test.c
	gcc -Wall -Werror -O2 -o ioctl_test ioctl_test.c

clean:
	make -C /usr/src/linux-headers-$(shell uname -r) M=$(PWD) clean
	rm -f *.o.cmd *.symvers *.order *.gch rw_test lseek_test ioctl_test
