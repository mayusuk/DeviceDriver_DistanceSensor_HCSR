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
#include "data.h"


struct thread_arg {
	int fd;
	int driver_number;

};

void* read_measurement(void* arg){

     struct thread_arg *argument = (struct thread_arg *)arg;
     int res,i;
     struct data measurement;

	if((res = read(argument->fd, &measurement, sizeof(struct data))) < 0) {
		printf("Error in starting the measurement\n");		
     	}
	sleep(4);
       printf("Current distance measurement - Time :- %llu  Distance :- %d\n", measurement.timestamp,  measurement.distance);

	if((res = read(argument->fd, &measurement, sizeof(struct data))) < 0) {
		printf("Error in starting the measurement\n");		
     	}
       printf("Current distance measurement - Time :- %llu  Distance :- %d\n", measurement.timestamp,  measurement.distance);

 
}


void* start_measurement(void* arg){

     int fd = (int*)arg;
     int res;
     int enable = 0;

     if((res = write(fd, &enable, sizeof(int))) < 0) {
	printf("Error in starting the measurement\n");		
     }
     sleep(4);
     if((res = write(fd, &enable, sizeof(int)))< 0) {
	printf("Error in starting the measurement\n");
     }
     if((res = write(fd, &enable, sizeof(int)))< 0) {
		printf("Error in starting the measurement\n");
     }
     if((res = write(fd, &enable, sizeof(int)))< 0) {
	printf("Error in starting the measurement\n");
     }
     
}

int main(int argc, char **argv){

    pthread_t *thread;
    int i,number_of_devices, thread_count, set_retval;
    struct pins *pin_conf;
    struct parameters *params;
    struct thread_arg *argument;
    int* fd;
    char *name;

    printf("Please enter the number of devices:-");
    scanf("%d", &number_of_devices);
    fd = malloc(sizeof(int)*number_of_devices);
    pin_conf= malloc(sizeof(struct pins)*number_of_devices);
    params = malloc(sizeof(struct parameters)*number_of_devices);
    thread = malloc(sizeof(pthread_t)*number_of_devices*2);
    argument = malloc(sizeof(struct thread_arg)*number_of_devices);

    for(i=0;i<number_of_devices;i++){
		printf("Please enter the echo pin for device %d:-", i);
	   	scanf("%d", &pin_conf[i].echo_pin);
		printf("Please enter the trigger pin for device %d:-", i);
	   	scanf("%d", &pin_conf[i].trigger_pin);
		printf("Please enter the number of samples for device %d:-", i);
	   	scanf("%d", &params[i].number_of_samples);
		printf("Please enter the delay for device %d:-", i);
	   	scanf("%d", &params[i].delta);	
    }

    for(i=0;i<number_of_devices;i++){
			if (!(name = malloc(sizeof(char)*40)))
		{
			printf("Bad Kmalloc\n");
			return -ENOMEM;
		}
		 memset(name, 0, sizeof(char)*40);
		snprintf(name, sizeof(char)*40, "/dev/%s_%d", DEVICE_NAME_PREFIX, i);
		fd[i] = open(name, O_RDWR);
		argument[i].fd = fd[i];
		argument[i].driver_number = i;

    }

	if(argc > 1 && strcmp("ioctl", argv[1]) == 0){

	    for(i=0;i<number_of_devices;i++){
			printf("%d %d %d %d", params[i].delta, params[i].number_of_samples, pin_conf[i].echo_pin, pin_conf[i].trigger_pin);
			set_retval = ioctl(fd[i], SET_PARAMETERS, (unsigned long)&params[i]);
			if(ioctl(fd[i], CONFIG_PINS, (unsigned long)&pin_conf[i]) < 0){
				printf("Error while setting pins\n");
				return -1;		
			}
	    }
	  
	}
    thread_count = 0;

    for(i=0;i<number_of_devices;i++){
	     pthread_create(&thread[thread_count++],NULL, start_measurement, (void*)&argument[i]);
             sleep(5);
             pthread_create(&thread[thread_count++],NULL, read_measurement, (void*)&fd[i]);
	     sleep(2);
    }

    for(i=0;i<number_of_devices;i++){
		close(fd[i]);
    }

    thread_count = 0;
    for(i=0;i<number_of_devices;i++){
	     pthread_join(thread[thread_count++],NULL);
    }


}
