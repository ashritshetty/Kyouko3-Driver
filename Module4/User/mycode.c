#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#define DEVICE_FILE_NAME "/dev/kyouko3"

#define VMODE _IOW(0xcc, 0, unsigned long)
#define BIND_DMA _IOW(0xcc, 1, unsigned long)
#define START_DMA _IOWR(0xcc, 2, unsigned long)
#define FIFO_QUEUE _IOWR(0xcc, 3, unsigned long)
#define FIFO_FLUSH _IO(0xcc, 4)
#define UNBIND_DMA _IOW(0xcc, 5, unsigned long)

#define FIFO_FLUSH_REG 0x3ffc

#define RASTER_PRIMITIVE 0x3000
#define RASTER_EMIT 0x3004

#define VERTEX_COORD 0x5000
#define VERTEX_COLOR 0x5010

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
  
  float x[3] = {0.0, -0.5, 0.5};
  float y[3] = {-0.5, 0.2, 0.2};
  float z[3] = {0.0, 0.0, 0.0};
  float w[3] = {1.0, 1.0, 1.0};
  
  float r[3] = {1.0, 0.0, 0.0};
  float b[3] = {0.0, 1.0, 0.0};
  float g[3] = {0.0, 0.0, 1.0};
  float a[3] = {0.0, 0.0, 0.0};
  
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

  ioctl(fd, VMODE, GRAPHICS_ON);
  
  entry.cmd = RASTER_PRIMITIVE;
  entry.value = 1;
  ioctl(fd, FIFO_QUEUE, &entry);
  
  for(i = 0; i < 3; i++)
  {
    //Writing X-coord
    entry.cmd = VERTEX_COORD;
    entry.value = (unsigned int*)x[i];
    ioctl(fd, FIFO_QUEUE, &entry);
    
    //Writing Y-coord
    entry.cmd = VERTEX_COORD+0x0004;
    entry.value = (unsigned int*)y[i];
    ioctl(fd, FIFO_QUEUE, &entry);
    
    //Writing Z-coord
    entry.cmd = VERTEX_COORD+0x0008;
    entry.value = (unsigned int*)z[i];
    ioctl(fd, FIFO_QUEUE, &entry);
    
    //Writing w-coord
    entry.cmd = VERTEX_COORD+0x000c;
    entry.value = (unsigned int*)w[i];
    ioctl(fd, FIFO_QUEUE, &entry);
    
    //Writing red color
    entry.cmd = VERTEX_COLOR;
    entry.value = (unsigned int*)r[i];
    ioctl(fd, FIFO_QUEUE, &entry);
    
    //Writing green color
    entry.cmd = VERTEX_COLOR+0x0004;
    entry.value = (unsigned int*)g[i];
    ioctl(fd, FIFO_QUEUE, &entry);
    
    //Writing blue color
    entry.cmd = VERTEX_COLOR+0x0008;
    entry.value = (unsigned int*)b[i];
    ioctl(fd, FIFO_QUEUE, &entry);
    
    //Writing alpha color
    entry.cmd = VERTEX_COLOR+0x000c;
    entry.value = (unsigned int*)a[i];
    ioctl(fd, FIFO_QUEUE, &entry);
    
    //Write 0 to vertex emit
    entry.cmd = RASTER_EMIT;
    entry.value = 0;
    ioctl(fd, FIFO_QUEUE, &entry);
  }
  
  //Write 0 to command/raster primitive
  entry.cmd = RASTER_PRIMITIVE;
  entry.value = 0;
  ioctl(fd, FIFO_QUEUE, &entry);
  
  //Write 0 to flush register
  entry.cmd = FIFO_FLUSH_REG;
  entry.value = 0;
  ioctl(fd, FIFO_QUEUE, &entry);
  
  ioctl(fd, FIFO_FLUSH, 0);
  
  sleep(10);
  
  ioctl(fd, VMODE, GRAPHICS_OFF);
  
  printf("[USER] Closing device : %s\n", DEVICE_FILE_NAME);
  close(fd);

  return 0;
}
