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

#define DMA_HEADER_SZ 0x00000004

struct fifo_entry{
    unsigned int cmd;
    unsigned int value;
}fifo_entry;

struct u_kyouko_device
{
  unsigned int *u_control_base;
  unsigned int *u_frame_buffer;
}kyouko3;

struct kyouko3_dma_header{
    unsigned int address : 14;
    unsigned int count : 10;
    unsigned int opCode : 8;
}k_dma_header;

unsigned int U_READ_REG(unsigned int reg)
{
  return *(kyouko3.u_control_base+(reg>>2));
}

void U_WRITE_FB(unsigned int reg, unsigned int value)
{
  *(kyouko3.u_frame_buffer+(reg)) = value;
}

void fillTriangle(unsigned int* temp_addr, int factor)
{
  float x[3] = {0.0, -0.5, 0.5};
  float y[3] = {-0.5, 0.2, 0.2};
  float z[3] = {0.0, 0.0, 0.0};
  float w[3] = {1.0, 1.0, 1.0};
  
  float r[3] = {1.0, 0.0, 0.0};
  float b[3] = {0.0, 1.0, 0.0};
  float g[3] = {0.0, 0.0, 1.0};
  float a[3] = {0.0, 0.0, 0.0};  
    
  int i;
  //Format is BGRXYZ
  for(i = 0; i < 3; i++)
  {     
    //Writing blue color
    *temp_addr = *(unsigned int*)&b[i];
    temp_addr++;
        
    //Writing green color
    *temp_addr = *(unsigned int*)&g[i];
    temp_addr++;
        
    //Writing red color
    *temp_addr = *(unsigned int*)&r[i];
    temp_addr++;
      
    //Writing X-coord
    *temp_addr = *(unsigned int*)&x[i] * factor;
    temp_addr++;
    
    //Writing Y-coord
    *temp_addr = *(unsigned int*)&y[i] * factor;
    temp_addr++;
    
    //Writing Z-coord
    *temp_addr = *(unsigned int*)&z[i] * factor;
    temp_addr++;
  }    
}

int main(int argc, char *argv[])
{
  int fd;
  int ret, i;
  unsigned int RAM_SIZE;
  struct fifo_entry entry;
  unsigned long dma_addr = 0;
  unsigned int* temp_addr;
  
  k_dma_header.address = 0x1045;
  k_dma_header.count = 0x0003;
  k_dma_header.opCode = 0x0014;
  
  int arr[] = {1, -1};
  
  printf("[USER] Opening device : %s\n", DEVICE_FILE_NAME);
  fd = open(DEVICE_FILE_NAME, O_RDWR);
  if(fd < 0)
  {
    printf("[USER] Cannot open device : %s\n", DEVICE_FILE_NAME);
    return 0;
  }

  ioctl(fd, VMODE, GRAPHICS_ON);
  
  //Calling BIND_DMA
  ret = ioctl(fd, BIND_DMA, &dma_addr);
  temp_addr = (unsigned int*)dma_addr;
  printf("DMA_ADDR2: %x   %p \n", *temp_addr, temp_addr);
  
  for(i = 0; i < 2; ++i)
  {
    //Writing dma header
    *temp_addr = *(unsigned int*)&k_dma_header;
    temp_addr++;  
    
    fillTriangle(temp_addr, arr[i]);
    dma_addr = 76;
    ioctl(fd, START_DMA, &dma_addr);
    temp_addr = (unsigned int*)dma_addr;
  }
  
  //Write 0 to flush register
  entry.cmd = FIFO_FLUSH_REG;
  entry.value = 0;
  ioctl(fd, FIFO_QUEUE, &entry);
  
  ioctl(fd, FIFO_FLUSH, 0);

  sleep(10);
  
  if(ret == 0){
    ioctl(fd, UNBIND_DMA, &dma_addr);    
  }
  ioctl(fd, VMODE, GRAPHICS_OFF);
  
  printf("[USER] Closing device : %s\n", DEVICE_FILE_NAME);
  close(fd);

  return 0;
}
