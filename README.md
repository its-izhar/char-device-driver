# Simple Character Device Driver

Please read this to check the implementation requirements: [Description.pdf](https://github.com/its-izhar/char-device-driver/blob/master/AssignmentDescription.pdf)

### To compile module and the test apps run:
```
make
```
### To clean run:
```
make clean
```
### To Insert the module in the kernel:
(This will also create the device nodes in the /dev directory with root permissions, named as /dev/mycdev0, /dev/mycdev1 ... upto one less than max_devices.)
```
sudo insmod asp_mycdev.ko max_devices=<num-of-desired-devices>
```
### To remove the module:
```
sudo rmmod asp_mycdev.ko
```
### To check the kernel log:
```
dmesg
```
