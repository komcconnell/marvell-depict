#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int SM_GPIO_PortRead_IOCTL(int port, int *value);
extern int SM_GPIO_PortWrite_IOCTL(int port, int value);

static const char *prog_name;

static void usage()
{
    printf("Usage: %s read/write port value\n", prog_name);
}

int main(int argc, char *argv[])
{
    int port, value;

    prog_name = strrchr(argv[0], '/');
    if (!prog_name)
        prog_name = argv[0];
    else prog_name++;

    if (argc < 3) {
        usage();
		return -1;
	}

    port  = strtoul(argv[2], NULL, 10);

    if (strcmp("read", argv[1]) == 0){
        SM_GPIO_PortRead_IOCTL(port, &value);
        printf("gpio port %d value is %d\n", port, value);
    } else if(strcmp("write", argv[1]) == 0){
        if (argc < 4) {
            usage();
		    return -1;
		}else {
            value  = strtoul(argv[3], NULL, 10);
            SM_GPIO_PortWrite_IOCTL(port, value);
            printf("gpio port %d value is %d\n", port, value);
        }
	}else {
	    usage();
		return -1;
	}

    return 0;
}
