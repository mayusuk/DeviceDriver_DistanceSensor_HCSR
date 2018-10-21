
#include <linux/ioctl.h>

#define IOCTL_APP_TYPE 80
#define CONFIG_PINS _IOWR(IOCTL_APP_TYPE, 1, struct pins)     // ioctl to config pins of HCSR

#define SET_PARAMETERS _IOWR(IOCTL_APP_TYPE, 2, struct parameters)     // ioctl to config pins of HCSR


#define MAX_BUFF_SIZE  5
#define DEVICE_NAME_PREFIX "hcsr"
#define CLASS_NAME "HCSR_DRV"

struct pins {
	int echo_pin;
	int trigger_pin;
	int gpio_echo_pin;
	int gpio_trigger_pin;
};

struct parameters {
	int number_of_samples;
	int delta;
};


