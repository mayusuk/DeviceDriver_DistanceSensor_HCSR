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

static struct hcsr_muxtable mux_table[24] = {
	{0, true, false, {{11,DONT_CARE},{32,0},{DONT_CARE,DONT_CARE},{DONT_CARE,DONT_CARE}},OUTPUT_FUNC},
	{0, true, false, {{11,DONT_CARE},{32,1},{DONT_CARE,DONT_CARE},{DONT_CARE,DONT_CARE}},INPUT_FUNC},
	{1, true, false, {{12,DONT_CARE},{28,0},{45,0},{DONT_CARE,DONT_CARE}},OUTPUT_FUNC},
	{1, true, false, {{12,DONT_CARE},{28,1},{45,0},{DONT_CARE,DONT_CARE}},INPUT_FUNC},
	{2, true, false, {{13,DONT_CARE},{34,0},{77,0},{DONT_CARE,DONT_CARE}}, OUTPUT_FUNC},
	{2, true, false, {{13,DONT_CARE},{34,1},{77,0},{DONT_CARE,DONT_CARE}},INPUT_FUNC},
	{4, true, false, {{6,DONT_CARE},{36,0},{DONT_CARE,DONT_CARE},{DONT_CARE,DONT_CARE}},OUTPUT_FUNC},
	{4, true, false, {{6,DONT_CARE},{36,1},{DONT_CARE,DONT_CARE},{DONT_CARE,DONT_CARE}},INPUT_FUNC},
	{5, true, false, {{0,DONT_CARE},{18,0},{66,0},{DONT_CARE,DONT_CARE}},OUTPUT_FUNC},
	{5, true, false, {{0,DONT_CARE},{18,1},{66,0},{DONT_CARE,DONT_CARE}},INPUT_FUNC},
	{6, true, false, {{1,DONT_CARE},{20,0},{68,0},{DONT_CARE,DONT_CARE}},OUTPUT_FUNC},
	{6, true, false, {{1,DONT_CARE},{20,1},{68,0},{DONT_CARE,DONT_CARE}},INPUT_FUNC},
	{9, true, false, {{4,DONT_CARE},{22,0},{70,0},{DONT_CARE,DONT_CARE}},OUTPUT_FUNC},
	{9, true, false, {{4,DONT_CARE},{22,1},{70,0},{DONT_CARE,DONT_CARE}},INPUT_FUNC},
	{10, true, false, {{10,DONT_CARE},{26,0},{74,0},{DONT_CARE,DONT_CARE}},OUTPUT_FUNC},
	{10, true, false, {{10,DONT_CARE},{26,1},{74,0},{DONT_CARE,DONT_CARE}},INPUT_FUNC},
	{11, true, false, {{5,DONT_CARE},{24,0},{44,0},{DONT_CARE,DONT_CARE}},OUTPUT_FUNC},
	{11, true, false, {{5,DONT_CARE},{24,1},{44,0},{DONT_CARE,DONT_CARE}},INPUT_FUNC},
	{12, true, false, {{15,DONT_CARE},{18,0},{DONT_CARE,DONT_CARE},{DONT_CARE,DONT_CARE}},OUTPUT_FUNC},
	{12, true, false, {{15,DONT_CARE},{18,1},{DONT_CARE,DONT_CARE},{DONT_CARE,DONT_CARE}},INPUT_FUNC},
	{13, true, false, {{7,DONT_CARE},{30,0},{46,0},{DONT_CARE,DONT_CARE}},OUTPUT_FUNC},
	{13, true, false, {{7,DONT_CARE},{30,1},{46,0},{DONT_CARE,DONT_CARE}},INPUT_FUNC},
    {14, true, false, {{48,DONT_CARE},{24,0},{44,0},{DONT_CARE,DONT_CARE}},OUTPUT_FUNC},
	{14, true, false, {{48,DONT_CARE},{24,1},{44,0},{DONT_CARE,DONT_CARE}},INPUT_FUNC}

};


void* get_used_pins(int pin, bool function){
	int i = 0;
	for(i=0;i<24;i++){
		if(function == INPUT_FUNC && !mux_table[i].is_interrupt){
			continue;
		}
		if(mux_table[i].pin == pin && mux_table[i].is_in_use && function == mux_table[i].function){
			mux_table[i].is_in_use = false;
			return (void *)&mux_table[i].config;
		}
	}
	return NULL;

}
bool check_pin(int pin, bool function){
	int i = 0;
	for(i=0;i<24;i++){
		if(function == INPUT_FUNC && !mux_table[i].is_interrupt){
			continue;
		}
		if(mux_table[i].pin == pin && !mux_table[i].is_in_use && function == mux_table[i].function){
			return true;
		}
	}
	return false;
}

void* get_pin(int pin, bool function){
	int i = 0;
	for(i=0;i<24;i++){
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
