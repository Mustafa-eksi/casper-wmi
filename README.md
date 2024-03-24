# ⚠️ I'm not updating this repo anymore
This kernel module is deprecated. Because new driver registers a platform_profile device
but it isn't possible to do that in a kernel module. If you want to get the latest
driver, you will have to compile kernel with my patch.
You can track latest patches from https://lore.kernel.org/platform-driver-x86/?q=casper-wmi
Also, this current driver registers a pwm device, but this is inaccurate (it should've been a platform_profile device). Using this pwm
device with userspace applications will cause problems.

# Linux keyboard backlight driver for Casper Excalibur laptops
I'm working on sending a patch to lkml. Until then, this works for me.

# How to build and install
```
$ make
$ sudo insmod casper-wmi.ko
```
