#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>			// contact the open(),close(),read(),write() and so on!

#define DEVICE_NAME	"/dev/jni-test"	//device point

int main(int argc,char **argv)
{
    int fd;
    int ret;

    printf("\n start gpio_led_driver test \r\n");
    
    fd = open(DEVICE_NAME,"O_RDWR");//Open device ,get the handle
    
    printf("fd = %d \n",fd);
 
    if(fd <= 0) //open fail
    {
    	printf("open device %s error \n",DEVICE_NAME);
    }
    else
    {
    	printf("heloo--------------------------------------\n");

       	write(fd, 0, 0);
		
        ret = close(fd); //close device
    }
    return 0;
}
