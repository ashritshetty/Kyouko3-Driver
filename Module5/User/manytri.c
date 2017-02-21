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

float translate(float triangle[6], float xscale, float yscale)
{
  float rtriangle[6];
  rtriangle[0] = triangle[0] + xscale;
  rtriangle[1] = triangle[1] + yscale;
  rtriangle[2] = triangle[2] + xscale;
  rtriangle[3] = triangle[3] + yscale;
  rtriangle[4] = triangle[4] + xscale;
  rtriangle[5] = triangle[5] + yscale;

  return rtriangle;
}

float rotate(float triangle[6], float sino)
{
  float coso = sqrt(1-(sino*sino));
  float rtriangle[6];
  rtriangle[0] = triangle[0]*coso - triangle[1]*sino;
  rtriangle[1] = triangle[0]*sino + triangle[1]*coso;
  rtriangle[2] = triangle[2]*coso - triangle[3]*sino;
  rtriangle[3] = triangle[2]*sino + triangle[3]*coso;
  rtriangle[4] = triangle[4]*coso - triangle[5]*sino;
  rtriangle[5] = triangle[4]*sino + triangle[5]*coso;

  return rtriangle;
}

float check(float triangle[6])
{
  int i;
  float rtriangle[6];
  for(i = 0; i < 6; i++)
  {
    if(triangle[i] < -1.0)
      rtriangle[i] = -1.0;
    else if(triangle[i] > 1.0)
      rtriangle[i] = 1.0;
    else
      rtriangle[i] = triangle[i];
  }

  return rtriangle;
}

void draw(unsigned int* temp_addr, float triangle[6])
{
  float x[3] = {triangle[0], triangle[2], triangle[4]};
  float y[3] = {triangle[1], triangle[3], triangle[5]};
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
    x[i] *= factor;
    *temp_addr = *(unsigned int*)&x[i];
    temp_addr++;

    //Writing Y-coord
    y[i] *= factor;
    *temp_addr = *(unsigned int*)&y[i];
    temp_addr++;

    //Writing Z-coord
    z[i] *= factor;
    *temp_addr = *(unsigned int*)&z[i];
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

  //int factor = -1.0;
  
  k_dma_header.address = 0x1045;
  k_dma_header.count = 0x0003;
  k_dma_header.opCode = 0x0014;

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
  printf("DMA_ADDR2: %x   %p \n", temp_addr, temp_addr);

  float i, j;
  float dtriangle[] = {x1, y1, x2, y2, x3, y3};
  for(i = -1.0, i < 1.0, i = i+scale)
  {
    for(j = -1.0, j < 1.0, j = j+scale)
    {
      dtriangle = translate(dtriangle, xscale, yscale);
      dtriangle = rotate(dtriangle, j);
      dtriangle = check(dtriangle);

      *temp_addr = *(unsigned int*)&k_dma_header;
      temp_addr++;

      draw(temp_addr, dtriangle);
      dma_addr = 76;
      ioctl(fd, START_DMA, &dma_addr);
      temp_addr = (unsigned int*)dma_addr;

      entry.cmd = FIFO_FLUSH_REG;
      entry.value = 0;
      ioctl(fd, FIFO_QUEUE, &entry);
      xscale = xscale + scale;
    }
    yscale = yscale + scale;
  }

  ioctl(fd, FIFO_FLUSH, 0);

  sleep(3);
  
  if(ret == 0){
    ioctl(fd, UNBIND_DMA, &dma_addr);    
  }
  ioctl(fd, VMODE, GRAPHICS_OFF);
  
  printf("[USER] Closing device : %s\n", DEVICE_FILE_NAME);
  close(fd);

  return 0;
}
