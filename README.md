# neutron-driver
a driver i made for linux, for now your going to have to compile by yourself. im going to be honest however, i used ai a bit, but most of it was made by me. have fun with this driver

i am not responsible if your computer crashes or anything, **THIS IS IN BETA** i repeat **BETA**

check the image.png file for the result of a testing program

im going to add more stuff, like changing from ioctl's to netlinks
im also going to add the following commands later:

**NEUTRON_HIDE_SOCKET**
- hides an IPC socket from a process
- its going to be a bit weird, i have two ideas
- a three end point system, so itll be like this
- user -> kernel -> target process
- or just using hiding a socket from a target process

**NEUTRON_HIDE_VMA_FLAGS**
- basically lets say i have a region thats RWX, and i want to hide it
- so i use this command to make it look like RW or RX

**NEUTRON_FIND_LIBRARY**
- more really a helper command
- if you want to see the address of a loaded library
- it will be quite useful for people who want to see offsets

**NEUTRON_DUMP_REGION**
- dump a 4096 byte sheet, from the address your looking at
- then you can use something like objdump to turn it into asm

**NEUTRON_HIDE_PROCESS/NEUTRON_ISOLATE_PROCESS**
- hides/isolates a process so other processes cannot see it

## IM SOON GOING TO PORT THIS TO WINDOWS! GET READY ##
