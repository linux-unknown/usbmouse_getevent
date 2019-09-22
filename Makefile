
CC=/home/assin/usb-mouse/linuxsdk-friendlyelec/prebuilts/gcc/linux-x86/aarch64/gcc-linaro-6.3.1-2017.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc

usbmouse_get_event:usbmouse_get_event.c
	${CC} -pthread usbmouse_get_event.c -o usbmouse_get_event
