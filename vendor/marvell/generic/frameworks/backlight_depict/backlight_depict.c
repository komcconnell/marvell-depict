
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int SM_GPIO_PortSetInOut(int port, int in);
extern int SM_GPIO_PortRead_IOCTL(int port, int *value);
extern int SM_GPIO_PortWrite_IOCTL(int port, int value);


// This is a hack to hide the pink screen prior to displaying the 
// Depict logo.  We do this by temporarily turning of the panel backlight
// for the first 2 seconds (during the time we'd normally see a pink screen).
//
// The "real" fix would be to properly initialize the Vx1 registers for
// the Depict/Arira board in the bootloader.  From discussions with Marvell,
// these are the registers:
//
//     1. Pol swap
//
//        a.      Please read & ensure 1st bytes of the following register content
//
//                i.      Reg: 0xF7E20168 = 0x000423fe      
//
//        b.      Please program following value into 1st byte of respective register
//
//                i.      Reg: 0xF7E20168 = 0x000427fe
//
// 
//     2. Lane swap
//
//        a.      Please read & ensure the following register content
//
//                i.      Reg: 0xF7C80558 = 0x4340000
//               ii.      Reg: 0xF7C8055C = 0x10000000
//              iii.      Reg: 0xF7C80560 = 0xEC6
//
//        b.      Please program following value into respective register
//
//                i.      Reg: 0xF7C80558 = 0x7B00000
//               ii.      Reg: 0xF7C8055C = 0x1000000E
//              iii.      Reg: 0xF7C80560 = 0x3


int main(int argc, char *argv[])
{
    int port = 38;
    int value = 0;

    //SM_GPIO_PortSetInOut(port, 0); // make sure this is set for output

    SM_GPIO_PortWrite_IOCTL(port, 0); // turn off panel backlight

    // Sleep while DEPICT logo starts up (this would normally be pink unless
    // the above registers are properly initialized prior to this)
    sleep(2);
    
    SM_GPIO_PortWrite_IOCTL(port, 1); // turn on panel backlight
    
    SM_GPIO_PortRead_IOCTL(port, &value);
    printf("backlight is: %d\n", value);
    
    return 0;
}

