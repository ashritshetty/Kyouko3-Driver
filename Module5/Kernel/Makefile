obj-m += mymod2.o

default:
	$(MAKE) -C /usr/src/linux M=$(PWD) modules
clean:
	rm *.ko
	rm *.o
	rm *.mod.c
	rm *.order
	rm *.symvers
