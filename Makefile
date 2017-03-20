# @Author: Izhar Shaikh <izhar>
# @Date:   2017-03-14T14:17:32-04:00
# @Email:  izharits@gmail.com
# @Filename: Makefile
# @Last modified by:   izhar
# @Last modified time: 2017-03-19T23:39:11-04:00
# @License: MIT



obj-m += asp_mycdev.o

all:
	make -C /usr/src/linux-headers-4.8.0-39-generic M=$(PWD) modules

clean:
	make -C /usr/src/linux-headers-4.8.0-39-generic M=$(PWD) clean
	rm -f *.o.cmd *.symvers *.order
