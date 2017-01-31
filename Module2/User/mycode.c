#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/mman.h>

#define DEVICE_FILE_NAME "/dev/kyouko3"

#define KYOUKO3_CONTROL_SIZE 65536
#define DEVICE_RAM 0x0020

struct u_kyouko_device
{
  unsigned int *u_control_base;
} kyouko3;

unsigned int U_READ_REG(unsigned int reg)
{
  return *(kyouko3.u_control_base+(reg>>2));
}

int main(int argc, char *argv[])
{
  int fd;
  int ret;

  printf("[USER] Opening device : %s\n", DEVICE_FILE_NAME);
  fd = open(DEVICE_FILE_NAME, O_RDWR);
  if(fd < 0)
  {
    printf("[USER] Cannot open device : %s\n", DEVICE_FILE_NAME);
    return 0;
  }
  
  kyouko3.u_control_base = mmap(0, KYOUKO3_CONTROL_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  ret = U_READ_REG(DEVICE_RAM);
  printf("Ram size in MB is: %d \n", ret);

  printf("[USER] Closing device : %s\n", DEVICE_FILE_NAME);
  close(fd);

  return 0;
}
