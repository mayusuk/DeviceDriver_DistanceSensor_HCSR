#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <asm-generic/errno.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>  
#include <linux/time.h> 
#include "hcsr.h"
#include "buffer.h"

#define MAX_BUFF_SIZE  5
#define DEVICE_NAME_PREFIX "hcsr"
#define CLASS_NAME "HCSR_DRV"


#define IOCTL_APP_TYPE 80
#define CONFIG_PINS _IOR(IOCTL_APP_TYPE, 1, struct pins)     // ioctl to config pins of HCSR

#define SET_PARAMETERS _IOR(IOCTL_APP_TYPE, 2, struct paramters)     // ioctl to config pins of HCSR


int number_of_device = 1;

module_param(number_of_device, int, S_IRUSR);

struct hcsr_dev {
	struct pins pin;
	struct paramters param;
	struct semaphore *sem;
	struct buff buffer;
	bool is_in_progress;
	struct miscdevice device;
};

struct all_devices {
	struct hcsr_dev **array_struct;
	dev_t *array_device_numbers;
};

unsigned long long get_tsc(void){
   unsigned a, d;

   __asm__ volatile("rdtsc" : "=a" (a), "=d" (d));

   return ((unsigned long long)a) | (((unsigned long long)d) << 32);
}


struct hcsr_dev *device_struct;
dev_t device_major_number;
struct all_devices table;
struct class *hcsr_class;
struct buff *all_samples;
struct semaphore all_samples_sem;
unsigned long long  tsc_raising, tsc_falling;

static irq_handler_t interrupt_handler(unsigned int irq, void *dev_id) { 
   
	struct hcsr_dev *hcsr_obj = (struct hcsr_dev)dev_id;
	double distance;
	struct 	data measurement;
	int check = gpio_get_value(hcsr_obj->pin.echo_pin);
	printk( "interrupt received (irq: %d)\n", irq);
	
	if (irq == gpio_to_irq(hcsr_obj->pin.echo_pin)) 
	{
		if (check ==0)
		{  
	        	printk("gpio pin is low\n");  
	 		tsc_falling=get_tsc();
			distance =  ((int)(tsc_raising-tsc_falling)/(139200)); 
			measurement.timestamp = tsc_falling;
			measurement.distance = distance;
			insert_buffer(all_samples,measurement);
			irq_set_irq_type(irq, IRQ_TYPE_EDGE_RISING);	
  		}
		else
		{
			printk("gpio pin is high\n"); 
			tsc_raising = get_tsc();
			irq_set_irq_type(irq, IRQ_TYPE_EDGE_FALLING);
		}
	}
	
   return (irq_handler_t) IRQ_HANDLED;  
}

int set_mux(struct pins* pins){
	//IO-0 trigger pin
	// IO -1 echo pin
	int gpio_11,gpio_32, gpio_12,gpio_28, gpio_45;
	int irqNumber, result;
	gpio_11 = gpio_request(11, "trigger_pin_value");
	gpio_32 = gpio_request(32, "trigger_pin_dir");
	gpio_12 = gpio_request(12, "echo_pin_value");
	gpio_28 = gpio_request(28, "echo_pin_dir");
	gpio_45 = gpio_request(45, "echo_pin_select");
	
	gpio_direction_output(11, 0);
	gpio_direction_output(32, 0);
	gpio_direction_output(28, 1);
	gpio_direction_input(12);
	gpio_direction_output(45, 0);

	irqNumber = gpio_to_irq(12);
	if(irqNumber < 0){
		//error	
	}
	
	result = request_irq(irqNumber,               		// The interrupt number requested 
		 (irq_handler_t) interrupt_handler, 	// The pointer to the handler function (above)
		 IRQF_TRIGGER_RISING,                 		// Interrupt is on rising edge
		 "ebb_gpio_handler",                  		// Used in /proc/interrupts to identify the owner
		 NULL);                               		// The *dev_id for shared interrupt lines, NULL here
	
	return 1;

	/*

	int gpio_trigger_pin, gpio_echo_pin;
	gpio_echo_pin = set_pin_conf(pins->echo_pin,INPUT_FUNC);
	gpio_trigger_pin= set_pin_conf(pins->trigger_pin,OUTPUT_FUNC);	
	pins->gpio_echo_pin = gpio_echo_pin;
	pins->gpio_trigger_pin = gpio_trigger_pin;
			
	*/

}

int set_pin_conf(int pin, bool func){

	struct conf *config = NULL;
	config = (struct conf*)get_pin(pin,func);
	if(conf == NULL){
		return -ENVAL;	
	}
	
	if(func == INPUT_FUNC){
		gpio_direction_input(config->gpio_pin.pin);		
	}
	if(config->directio_pin.pin != DONT_CARE){
		gpio_direction_output(config->directio_pin.pin, config->directio_pin.level);
	}
	if(config->function_pin_1.pin != DONT_CARE){
		gpio_direction_output(config->function_pin_1.pin, config->function_pin_1.level);
	}
	if(config->function_pin_2.pin != DONT_CARE){
		gpio_direction_output(config->function_pin_2.pin, config->function_pin_2.level);
	}

	return 	config->gpio_pin.pin;

}


static int start_sampling(void *handle)
{
	struct hcsr_dev *hcsr_obj = (struct hcsr_dev)handle;
	int counter = 0;

	printk("entered thread\n");
	do{
		gpio_set_value_cansleep(hcsr_obj->pin.trigger_pin, 0);
		gpio_set_value_cansleep(hcsr_obj->pin.trigger_pin, 1);
		udelay(30);
   		gpio_set_value_cansleep(hcsr_obj->pin.trigger_pin, 0);									
		mdelay(hcsr_obj->param.delta);
		counter++;      															
	}while(!kthread_should_stop() || counter < hcsr_obj->param.number_of_samples + 2);
	udelay(30);
	hscr_obj->is_in_progress = 0;

	return 0;
}

ssize_t start_measurement(struct file *file, const char *buff, size_t size, loff_t *lt){
	
	struct hcsr_dev *device_data = file->private_data;
	int *input;
	int i, total_distance;
	struct 	data measurement;
	unsigned long long timestamp;
	if (!(input= kmalloc(sizeof(int), GFP_KERNEL)))
	{
		printk("Bad Kmalloc\n");
		return -ENOMEM;
	}

	if (copy_from_user(input, buff, sizeof(int))
		return -EFAULT;

	if(device_data->is_in_progress){
		printk(KERN_INFO "On-Going Measurement");
		return -EINVAL;
	}else{
		if(input  == 0) {
			// write code to handle clearing of buffer
		}
		device_data->is_in_progress = 1;
                kthread_run(&start_sampling,(void *)device_data, "start_sampling");
		
		do{
		}while(all_samples->tail < device_data->param.no_of_samples + 1);
		
		total_distance = 0;
		timestamp = get_tsc();
		for(i = 1;i < device_data->param.no_of_samples+1; i++){
			distance += read_buffer(all_samples, all_samples_sem);
		}
		
		measurement.timestamp = timestamp;
		measurement.distance = total_distance/device_data->param.no_of_samples;
		insert_buffer(device_data->buffer, measurement, device_data->sem);
				
	}
}


ssize_t read_buffer(struct file *file, char *buff, size_t size, loff_t *lt) {
	struct hcsr_dev *device_data = file->private_data;
	if(device_data->is_in_progress){
		struct data readings = read_buffer(&device_data->buffer);
		printk(KERN_INFO "READING from buffer %lf", readings.distance);
	}else{
	    start_measurement(file, buff, size, lt);	
	}
}


int device_open(struct inode *inode, struct file *file){
	struct hcsr_dev *hcsr_devp = NULL;
	struct miscdevice *misc_dev = NULL;
	misc_dev = container_of(inode->i_fop, struct miscdevice, fops);
	hcsr_devp = container_of(misc_dev, struct hcsr_dev, device);
	file->private_data = hcsr_devp;
	printk(KERN_INFO "%s device is opened\n", hcsr_devp->device.name);
	return 0;
}


bool validate_pins(struct pins *pin){
	if(pin->echo_pin < 0 || pin->trigger_pin < 0){
		return false;
	}	
	return true;
}
/**

	IOCTL for Seeting pins and configuration of HCSR device

*/
long ioctl_handle(struct file *file, unsigned int command, unsigned long ioctl_param){
	struct pins *pin_conf;
	struct paramters* param;
	struct hcsr_dev *device_data = file->private_data;
	switch(command){
		case CONFIG_PINS:
					pin_conf = kmalloc(sizeof(struct pins), GFP_KERNEL);
					if(!validate_pins(pin_conf)){
						return -EINVAL;					
					}
					copy_from_user(pin_conf, (struct pins*) ioctl_param, sizeof(pin_conf));
					device_data->pin.echo_pin = pin_conf->echo_pin;
					device_data->pin.trigger_pin = pin_conf->trigger_pin;
					kfree(pin_conf);
					break;
		case SET_PARAMETERS:
					param = kmalloc(sizeof(struct paramters), GFP_KERNEL);
					copy_from_user(param, (struct paramters*) ioctl_param, sizeof(param));
					device_data->param.number_of_samples = param->number_of_samples;
					device_data->param.delta = param->delta;
					set_mux(&device_data->pin);
					init_buffer(all_samples, param->number_of_samples, all_samples_sem);
					kfree(param);
					break;
		default: return -EINVAL;		
	}

	
	return 1;
}


struct file_operations fops = {

	.open = device_open,
	.write = start_measurement,
	.read = read_buffer,
	.unlocked_ioctl = ioctl_handle
};

void init_buffer(struct buff* buffer){
	buffer->head = 0;
	buffer->tail = 0;
}

int init_gpio_module(void){
	
	int i, error;
	char name[20];
	// table.array_struct = (struct hcsr_dev**)kmalloc(sizeof(struct hcsr_dev *)*number_of_device, GFP_KERNEL);
	// table.array_device_number = kmalloc(sizeof(dev_t)*number_of_devices, GFP_KERNEL);
	hcsr_class = class_create(THIS_MODULE, CLASS_NAME);
	snprintf(name, sizeof name, "%s",DEVICE_NAME_PREFIX);

	device_struct = kmalloc(sizeof(struct hcsr_dev), GFP_KERNEL);
	if(!device_struct) {
	   printk(KERN_DEBUG "ERROR while allocating memory");
	   return -1;
	}
	
	device_struct->device.minor = MISC_DYNAMIC_MINOR;
	device_struct->device.name = name;
	device_struct->device.fops = &fops;
	
	error = misc_register(&device_struct->device);
	if(error){
			
	}
	
	init_buffer(&device_struct->buffer);
	all_samples = kmalloc(sizeof(struct buff), GFP_KERNEL);
	printk(KERN_INFO "Driver %s is initialised\n", CLASS_NAME);
	return 0;
	
}

void exit_gpio_driver(void){
	
	printk(KERN_INFO "Removing device");

}

module_init(init_gpio_module);
module_exit(exit_gpio_driver);
MODULE_LICENSE("GPL v2");

