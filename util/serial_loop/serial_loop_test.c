
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
              
int main(int argc, char **argv)

{
  int fd;
  
  fd = open("/dev/ttyS2", O_RDWR);
  if (fd == -1) {
    printf("Error in open()\n");
    return -1;
  }
  
  write(fd, "blah", 4);
  
  close(fd);
  
  return 0;
} 
