#include <stdbool.h>

#define DONT_CARE -1
#define INPUT_FUNC true
#define OUTPUT_FUNC false

struct pin_conf{
	int pin;
	int level;
};

struct conf {
	struct pin_conf gpio_pin;
	struct pin_conf direction_pin;
	struct pin_conf function_pin_1;
	struct pin_conf function_pin_2;
};

struct hcsr_muxtable {
	int pin;
	bool is_interrupt;
	bool is_in_use;
	struct conf config;
	bool function;

};

struct hcsr_muxtable mux_table[6] = {
	{0, true, false, {{11,DONT_CARE},{32,0},{DONT_CARE,DONT_CARE},{DONT_CARE,DONT_CARE}},OUTPUT_FUNC},
	{0, true, false, {{11,DONT_CARE},{32,1},{DONT_CARE,DONT_CARE},{DONT_CARE,DONT_CARE}},INPUT_FUNC},
	{1, true, false, {{12,DONT_CARE},{28,0},{45,0},{DONT_CARE,DONT_CARE}},OUTPUT_FUNC},
	{1, true, false, {{12,DONT_CARE},{28,1},{45,0},{DONT_CARE,DONT_CARE}},INPUT_FUNC},
	{2, true, false, {{13,DONT_CARE},{34,0},{77,0},{DONT_CARE,DONT_CARE}}, OUTPUT_FUNC},
	{2, true, false, {{13,DONT_CARE},{34,0},{77,0},{DONT_CARE,DONT_CARE}},INPUT_FUNC}
};


void* get_pin(int pin, bool function){
	int i = 0;
	for(i=0;i<6;i++){
		if(function == INPUT_FUNC && !mux_table[i].is_interrupt){
			continue;
		}
		if(mux_table[i].pin == pin && !mux_table[i].is_in_use && function == mux_table[i].function){
			mux_table[i].is_in_use = true;
			return (void *)&mux_table[i].config;
		}
	}
	return NULL;
}
