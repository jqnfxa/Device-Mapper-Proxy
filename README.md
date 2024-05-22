# Device Mapper Proxy Module

This repository contains the source code for a Linux kernel module named "Device Mapper Proxy". This module acts as a proxy between the host system and a block device, providing detailed statistics about read and write operations.

## System Requirements

This module has been tested on Ubuntu 24.04 with kernel on 6.8.0-31-generic version.

## Supported Statistics

- Number of write requests
- Number of read requests
- Average block size per write
- Average block size per read
- Total number of requests
- Average block size

## Building the Module

Before building the module, ensure you have the necessary tools installed, such as `gcc`, `make`, and the Linux kernel headers for your current kernel version.

1. Clone this repository to your local machine.
2. Navigate to the directory containing the source code.
3. Run `make` to compile the module.

## Installing the Module

After compiling the module, you need to load it into the kernel:
```
sudo insmod dmp.ko
```

Check if module is installed:
```
sudo lsmod | grep dmp
dmp                    12288  0
```

## Creating a Test Block Device

Create a test block device using `dmsetup`. Replace `$size` with the desired size of the device (100 for example).:
```
sudo dmsetup create zero1 --table "0 $size zero"
```

Verify the creation of the device:
```
ls -al /dev/mapper/*
lrwxrwxrwx 1 root root       7 May 22 19:53 /dev/mapper/zero1 -> ../dm-0
```

## Creating Our Proxy Device

Create a proxy device that uses the previously created block device (again, replace `$size` with the size of the device):
```
sudo dmsetup create dmp1 --table "0 $size dmp /dev/mapper/zero1"
```

Verify the creation of the proxy device:
```
ls -al /dev/mapper/*
lrwxrwxrwx 1 root root       7 May 22 19:55 /dev/mapper/dmp1 -> ../dm-1
lrwxrwxrwx 1 root root       7 May 22 19:53 /dev/mapper/zero1 -> ../dm-0
```

## Write and Read Operations

Perform write and read operations to generate statistics:

### Writing to the Device
```
sudo dd if=/dev/random of=/dev/mapper/dmp1 bs=4k count=1
1+0 records in
1+0 records out
4096 bytes (4.1 kB, 4.0 KiB) copied, 0.000266703 s, 15.4 MB/s
```

### Reading from the Device
```
sudo dd of=/dev/null if=/dev/mapper/dmp1 bs=4k count=1
1+0 records in
1+0 records out
4096 bytes (4.1 kB, 4.0 KiB) copied, 0.000197436 s, 20.7 MB/s
```

## Viewing Statistics

View the collected statistics through the sysfs interface:
```
cat /sys/module/dmp/stat/volumes
read:
	reqs: 45
	avg size: 3439
write:
	reqs: 2
	avg size: 2048
total:
	reqs: 47
	avg size: 3413
user@user-vm:~/Documents
```
This will display the number of read and write requests, average block sizes, and total requests.

## Unloading the Module

Once you have finished testing, simply remove associated device and unload the module from the kernel:
```
sudo dmsetup remove dmp1
sudo rmmod dmp
```

## License

This project is licensed under the GPL license.
