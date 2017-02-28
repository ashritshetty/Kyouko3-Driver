#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <time.h>

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

#define GRAPHICS_ON 1
#define GRAPHICS_OFF 0

#define DMA_HEADER_SZ 0x00000004

static int fd;

struct fifo_entry{
    unsigned int cmd;
    unsigned int value;
};

struct u_kyouko_device{
  unsigned int *u_control_base;
  unsigned int *u_frame_buffer;
}kyouko3;

struct kyouko3_dma_header{
    unsigned int address : 14;
    unsigned int count : 10;
    unsigned int opCode : 8;
}k_dma_header;

void intHandler(int signum)
{
  printf("[USER] Clearing DMA Buffer\n");
  ioctl(fd, FIFO_FLUSH, 0);
  ioctl(fd, UNBIND_DMA, 0);    
  printf("[USER] Turning off graphics mode\n");
  ioctl(fd, VMODE, GRAPHICS_OFF);
  printf("[USER] Closing device : %s\n", DEVICE_FILE_NAME);
  close(fd);
  exit(0);
}

unsigned int U_READ_REG(unsigned int reg)
{
  return *(kyouko3.u_control_base+(reg>>2));
}

void U_WRITE_FB(unsigned int reg, unsigned int value)
{
  *(kyouko3.u_frame_buffer+(reg)) = value;
}

void check(float triangle[], float* rtriangle)
{
  int i;

  for(i = 0; i < 6; i++)
  {
    if(triangle[i] < -1.0)
      rtriangle[i] = -0.9;
    else if(triangle[i] > 1.0)
      rtriangle[i] = 0.9;
    else
      rtriangle[i] = triangle[i];
  }
}

void draw(unsigned int* temp_addr, float triangle[], float color[])
{
  int i;

  float x[3] = {triangle[0], triangle[2], triangle[4]};
  float y[3] = {triangle[1], triangle[3], triangle[5]};
  float z[3] = {0.0, 0.0, 0.0};
  float w[3] = {1.0, 1.0, 1.0};

  float r[3] = {color[0],0.0,0.0};
  float g[3] = {0.0,color[1],0.0};
  float b[3] = {0.0,0.0,color[2]};
  float a[3] = {0.0,0.0,0.0};

  //Format is BGRXYZ
  for(i = 0; i < 9; i++)
  {
    //Writing blue color
    *temp_addr = *(unsigned int*)&b[i%3];
    temp_addr++;

    //Writing green color
    *temp_addr = *(unsigned int*)&g[i%3];
    temp_addr++;

    //Writing red color
    *temp_addr = *(unsigned int*)&r[i%3];
    temp_addr++;

    //Writing X-coord
    *temp_addr = *(unsigned int*)&x[i%3];
    temp_addr++;

    //Writing Y-coord
    *temp_addr = *(unsigned int*)&y[i%3];
    temp_addr++;

    //Writing Z-coord
    *temp_addr = *(unsigned int*)&z[i%3];
    temp_addr++;
  }
}

void set_intHandler()
{
  signal(SIGINT, intHandler);
  signal(SIGTERM, intHandler);
  signal(SIGQUIT, intHandler);
  signal(SIGHUP, intHandler);
}

int main(int argc, char *argv[])
{
  set_intHandler();
 
  if(argc != 2)
  {
    printf("[USER] Usage : ./mycode <number of triangles> \n");
    return 0;
  }

  int ret, i, n;
  unsigned int RAM_SIZE;
  unsigned int* temp_addr;
  unsigned long dma_addr = 0;
  struct fifo_entry entry;
 
  float color[3];
  float triangle[6];
  float rtriangle[6];

  n = atoi(argv[1]);
  
  k_dma_header.address = 0x1045;
  k_dma_header.count = 0x0009;
  k_dma_header.opCode = 0x0014;

  printf("[USER] Opening device : %s\n", DEVICE_FILE_NAME);
  fd = open(DEVICE_FILE_NAME, O_RDWR);
  if(fd < 0)
  {
    printf("[USER] Cannot open device : %s\n", DEVICE_FILE_NAME);
    return 0;
  }
  
  ret = ioctl(fd, BIND_DMA, &dma_addr);
  temp_addr = (unsigned int*)dma_addr;
  printf("[USER] DMA_ADDR2: %x   %p \n", temp_addr, temp_addr);

  ioctl(fd, VMODE, GRAPHICS_ON);

  srand(time(NULL));

  for(i = 0; i < n; i++)
  {
    float x1rand = (float)rand() / (float)RAND_MAX;
    float y1rand = (float)rand() / (float)RAND_MAX;
    float x2rand = (float)rand() / ((float)RAND_MAX*10);
    float y2rand = (float)rand() / ((float)RAND_MAX*10);
    float x3rand = (float)rand() / ((float)RAND_MAX*10);
    float y3rand = (float)rand() / ((float)RAND_MAX*10);

    triangle[0] = -1.0 + (2*x1rand);
    triangle[1] = -1.0 + (2*y1rand);
    triangle[2] = triangle[0] + x2rand;
    triangle[3] = triangle[1] + y2rand;
    triangle[4] = triangle[0] + x3rand;
    triangle[5] = triangle[1] + y3rand;

    color[0] = (float)rand() / (float)RAND_MAX;
    color[1] = (float)rand() / (float)RAND_MAX;
    color[2] = (float)rand() / (float)RAND_MAX;

    check(triangle, rtriangle);

    *temp_addr = *(unsigned int*)&k_dma_header;
    temp_addr++;

    draw(temp_addr, rtriangle, color);
    dma_addr = 76;
    ioctl(fd, START_DMA, &dma_addr);
    temp_addr = (unsigned int*)dma_addr;

    entry.cmd = FIFO_FLUSH_REG;
    entry.value = 0;
    ioctl(fd, FIFO_QUEUE, &entry);
  }
  
  ioctl(fd, FIFO_FLUSH, 0);

  sleep(3);
  
  if(ret == 0)
  {
    printf("[USER] Unbinding DMA buffers \n");
    ioctl(fd, UNBIND_DMA, &dma_addr);    
  }
 
  ioctl(fd, VMODE, GRAPHICS_OFF);

  printf("[USER] Closing device : %s\n", DEVICE_FILE_NAME);
  close(fd);

  return 0;
}
