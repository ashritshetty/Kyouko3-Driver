#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVICE_FILE_NAME "/dev/kyouko3"

int main(int argc, char *argv[])
{
  int file_desc;

  printf("[USER] Opening device : %s\n", DEVICE_FILE_NAME);
  file_desc = open(DEVICE_FILE_NAME, O_RDWR);
  if(file_desc < 0)
  {
    printf("[USER] Cannot open device : %s\n", DEVICE_FILE_NAME);
    return 0;
  }
  
  printf("[USER] Closing device : %s\n", DEVICE_FILE_NAME);
  close(file_desc);

  return 0;
}
