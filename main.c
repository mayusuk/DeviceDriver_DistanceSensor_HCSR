#include<stdio.h>
#include "muxtable.h"
int main(){
	

	struct conf *config;
	config = (struct conf*)get_pin(1, INPUT_FUNC);
	printf("*****%d", config->gpio_pin.pin);

	return 0;
}
