#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#define DEVICE_FILE_NAME "/dev/kyouko3"

#define FIFO_FLUSH_REG 0x3ffc

#define VMODE _IOW(0xcc, 0, unsigned long)
#define BIND_DMA _IOW(0xcc, 1, unsigned long)
#define START_DMA _IOWR(0xcc, 2, unsigned long)
#define FIFO_QUEUE _IOWR(0xcc, 3, unsigned long)
#define FIFO_FLUSH _IO(0xcc, 4)
#define UNBIND_DMA _IOW(0xcc, 5, unsigned long)

#define KYOUKO3_CONTROL_SIZE 65536
#define DEVICE_RAM 0x0020

//Defines to keep track of graphic on/off
#define GRAPHICS_ON 1
#define GRAPHICS_OFF 0

struct fifo_entry{
    unsigned int cmd;
    unsigned int value;
}fifo_entry;

struct u_kyouko_device
{
  unsigned int *u_control_base;
  unsigned int *u_frame_buffer;
} kyouko3;

unsigned int U_READ_REG(unsigned int reg)
{
  return *(kyouko3.u_control_base+(reg>>2));
}

void U_WRITE_FB(unsigned int reg, unsigned int value)
{
  *(kyouko3.u_frame_buffer+(reg)) = value;
}

int main(int argc, char *argv[])
{
  int fd;
  int ret, i;
  unsigned int RAM_SIZE;
  struct fifo_entry entry;
  entry.cmd = FIFO_FLUSH_REG;
  entry.value = 0;
  
  printf("[USER] Opening device : %s\n", DEVICE_FILE_NAME);
  fd = open(DEVICE_FILE_NAME, O_RDWR);
  if(fd < 0)
  {
    printf("[USER] Cannot open device : %s\n", DEVICE_FILE_NAME);
    return 0;
  }
  
  kyouko3.u_control_base = mmap(0, KYOUKO3_CONTROL_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  
  RAM_SIZE = U_READ_REG(DEVICE_RAM);
  printf("[USER] Ram size in MB is: %d \n", RAM_SIZE);
  
  RAM_SIZE = RAM_SIZE*1024*1024;
  kyouko3.u_frame_buffer = mmap(0, RAM_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0x80000000);

  printf("Virtual address: %p", kyouko3.u_frame_buffer);  

  ioctl(fd, VMODE, GRAPHICS_ON);
  
  for(i = 200*1024; i < 201*1024; i++)
  {
      U_WRITE_FB(i, 0xff000000);
  }
  
  ioctl(fd, FIFO_QUEUE, &entry);
  ioctl(fd, FIFO_FLUSH, 0);
  
  sleep(10);
  
  ioctl(fd, VMODE, GRAPHICS_OFF);
  
  printf("[USER] Closing device : %s\n", DEVICE_FILE_NAME);
  close(fd);

  return 0;
}
