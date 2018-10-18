#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include "hcsr.h"

int main(int argc, char **argv){


    int fd,set_retval;
    struct parameters param = {.number_of_samples = 4, .delta = 2};
    struct pins pin_conf = {.trigger_pin = 0, .echo_pin = 1, .gpio_echo_pin= 0, .gpio_trigger_pin=0};
    char buff[20];
    fd = open("/dev/HCSR_DEVICE", O_RDWR);
    if(strcmp("ioctl", argv[1]) == 0){
    printf("IOCTL");
        set_retval = ioctl(fd, SET_PARAMETERS, (unsigned long)&param);
        set_retval = ioctl(fd, CONFIG_PINS, (unsigned long)&pin_conf);
        printf("ioctl error %d\n", set_retval);
    }else if(strcmp("write", argv[1]) == 0){
        write(fd, buff, strlen(buff));
    }
    close(fd);
	return 0;
}
