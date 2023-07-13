obj-m += mutex_block.o

all:
	make -C /usr/lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /usr/lib/modules/$(shell uname -r)/build M=$(PWD) clean

install:
	sudo insmod mutex_block.ko

uninstall:
	sudo rmmod mutex_block.ko

list:
	sudo lsmod

