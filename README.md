# Data Transfer Application for MicroZed w/ Embedded Linux
# v2 - C++ Port

![Diagram](https://github.com/christopherbate/MZNTLogger/blob/master/MZNT%20Diagram.png)

## Application Setup 
0. Must be running the NT/MicroZed with latest version of Petalinux built for this project.
1. Ensure that the startup script is located in /etc/init.d/logger-init.

## Running the Logger - Command Line Options.

1. To run the logger, you need to have downloaded it from Xilinx SDK or transfered it at build time, or manually copied the executable to USB/SD

2. Command line options as follows, along with their defaults if they are not specified. Flags are by default, off.
  * -o <output path> Default: /mnt/IF.bin
  * -s <num seconds to run> Default: 3600 (1hr)
  * -c <path to config file> Default: /etc/ConfigSet10.txt
  * -t <threshold for resampling, given as hex> Default: 0x04000000
  * -a Flag to turn on the counter instead of RF data.
  * -n Flag to turn off resampling.

## A tour of the components:

Class | Description
--- | ---
HardwareManager | HardwareManager class contains all the functions for initializing NT, various FPGA-specific items for this application. At invocation of "Start()", spawns 2 threads to manage the DMA and writer IF data to file. Owned by LoggerController
LoggerController | This is the main controller used to start/stop the entire node. Owns HW Manager, MQTTController.
MQTTController | Manages connecting to MQTT broker, publishing messages, and subscribing to topics. Owned by LoggerController.
ThreadSafeQueue| Contains thread-safe wrapper around the standard STL queue.

### Formatting an SD Card for Ext3 FS and boot - For Prepping SD Card for Petalinux

Use Linux with fdisk, start with an SD card with no partitions and compeltely unmounted. If the SD card has partitions, use dd to write ~2MB of '/dev/zero' to the beginning of the disk to overwrite the table.

1. sudo fdisk /dev/(SDCARD)
2. Select 'n' for new partition table
3. Select 'p' then '1' then enter '4194034' as first sector (this leaves 4MB of free space at the beginning as required by Petalinux documentation)
4. Select '+100M' for size.
5. Enter 'p' to print the partition table. There should now be one partition. Record the 'End' number
6. Enter 'a' to make this partition bootable.
7. Select 'n' to make another partition, followed by "p", "2".
8. For the sector start, type the "end" number + 1 that you recorded for the first partition. Be sure to add 1 to it to create the new start sector for the new partition. Don't accept the default because it will try to put this partition in the free space.
9. For the sector end, select the default. This should max out the space.
10. Print the changes and then select 'w' to write to disk.
