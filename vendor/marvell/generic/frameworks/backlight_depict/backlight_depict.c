
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int SM_GPIO_PortRead_IOCTL(int port, int *value);
extern int SM_GPIO_PortWrite_IOCTL(int port, int value);


int main(int argc, char *argv[])
{
    int port = 38;
    int value = 0;

    sleep(3);
    
    SM_GPIO_PortWrite_IOCTL(port, 1);
    
    SM_GPIO_PortRead_IOCTL(port, &value);
    
    printf("backlight is: %d\n", value);
    
    return 0;
}

