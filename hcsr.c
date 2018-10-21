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
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <asm-generic/errno.h>
#include <linux/kthread.h>
#include <linux/time.h>
#include <asm/i387.h>

#include "hcsr.h"
#include "buffer.h"
#include "muxtable.h"

int number_of_device = 1;

module_param(number_of_device, int, S_IRUSR);

struct hcsr_dev {
    	struct miscdevice device;
	struct pins pin;
	struct parameters param;
	struct buff buffer;
	bool is_in_progress;
	struct buff all_samples;
	struct hcsr_dev *next;
	 spinlock_t inprogress_lock;
};

unsigned long long get_tsc(void){
   unsigned a, d;

   __asm__ volatile("rdtsc" : "=a" (a), "=d" (d));

   return ((unsigned long long)a) | (((unsigned long long)d) << 32);
}

unsigned long long  tsc_raising, tsc_falling;
struct timespec time_raising, time_falling;
static struct hcsr_dev* HEAD;

static irq_handler_t interrupt_handler(unsigned int irq, void *dev_id, struct pt_regs *regs) {

    //printk(KERN_INFO"INTERRUPT HANDLER\n");
	struct hcsr_dev *hcsr_obj = (struct hcsr_dev*)dev_id;
	int distance;
	long difference;
	struct 	data measurement;
	int check = gpio_get_value(hcsr_obj->pin.gpio_echo_pin);

	if (irq == gpio_to_irq(hcsr_obj->pin.gpio_echo_pin))
	{
		if (check ==0)
		{
            //printk("gpio pin is low\n");
	 		tsc_falling=get_tsc();
			getnstimeofday(&time_falling);
			difference =  (time_falling.tv_nsec - time_raising.tv_nsec)/1000;
			distance = difference / 58;
			measurement.timestamp = tsc_falling;
			measurement.distance = distance;
			insert_buffer(&hcsr_obj->all_samples, measurement, &hcsr_obj->all_samples.sem, &hcsr_obj->all_samples.Lock);
			irq_set_irq_type(irq, IRQ_TYPE_EDGE_RISING);
  		}
		else
		{
		//	printk("gpio pin is high\n");
			tsc_raising = get_tsc();
			getnstimeofday(&time_raising);
			irq_set_irq_type(irq, IRQ_TYPE_EDGE_FALLING);
			
		}
	}

   return (irq_handler_t) IRQ_HANDLED;
}


int set_interrupt(void *device_struct){

    struct hcsr_dev *hcsr_obj = (struct hcsr_dev*)device_struct;
    int irqNumber, result;
    irqNumber = gpio_to_irq(hcsr_obj->pin.gpio_echo_pin);
	if(irqNumber < 0){
		printk("GPIO to IRQ mapping failure \n" );
      		return -1;
	}
    printk(KERN_INFO "IRQNUMBER is %d\n", irqNumber);

    result = request_irq(irqNumber,               	// The interrupt number requested
		 (irq_handler_t) interrupt_handler, 	    // The pointer to the handler function (above)
		 IRQF_TRIGGER_RISING,                 		// Interrupt is on rising edge
		 "ebb_gpio_handler",                  		// Used in /proc/interrupts to identify the owner
		 (void*)hcsr_obj);                     // The *dev_id for shared interrupt lines, NULL here

    if(result < 0){
         printk("Irq Request failure\n");
		return -1;
    }
    return 0;
}

int release_irq(void* handle)
{
	struct hcsr_dev *hcsr_obj = (struct hcsr_dev*)handle;
	printk(KERN_INFO "Releasing the interrupt %d on pin %d\n", gpio_to_irq(hcsr_obj->pin.gpio_echo_pin), hcsr_obj->pin.gpio_echo_pin);
	free_irq(gpio_to_irq(hcsr_obj->pin.gpio_echo_pin), handle );
	return 0;
}


int set_pin_conf(int pin, bool func){

	struct conf *config = NULL;
	config = (struct conf*)get_pin(pin,func);
	if(config == NULL){
		return -EINVAL;
	}

	if(func == INPUT_FUNC){
        	printk(KERN_INFO "Setting gpio %d as input\n", config->gpio_pin.pin);
       		gpio_request(config->gpio_pin.pin, "Input");
		gpio_direction_input(config->gpio_pin.pin);
	}else{
		printk(KERN_INFO "Setting gpio %d as trigger\n", config->gpio_pin.pin);
       		gpio_request(config->gpio_pin.pin, "trigger");
		gpio_direction_output(config->gpio_pin.pin, 0);
	}
	if(config->direction_pin.pin != DONT_CARE){
        printk(KERN_INFO "Setting direction gpio  %d\n", config->direction_pin.pin);
		gpio_request(config->direction_pin.pin, "Direction");
		gpio_direction_output(config->direction_pin.pin, config->direction_pin.level);
	}
	if(config->function_pin_1.pin != DONT_CARE){
        printk(KERN_INFO "Setting function 1 gpio  %d\n", config->function_pin_1.pin);
        	gpio_request(config->function_pin_1.pin, "function_1");
		gpio_direction_output(config->function_pin_1.pin, config->function_pin_1.level);
	}
	if(config->function_pin_2.pin != DONT_CARE){
        	printk(KERN_INFO "Setting function 1 gpio  %d\n", config->function_pin_2.pin);
        	gpio_request(config->function_pin_2.pin, "function_2");
		gpio_direction_output(config->function_pin_2.pin, config->function_pin_2.level);
	}

	return 	config->gpio_pin.pin;

}

int mux_free_pins(int pin, bool func){

	struct conf *config = NULL;
	config = (struct conf*)get_used_pins(pin,func);
	if(config == NULL){
		return -EINVAL;
	}

	printk(KERN_INFO "Freeing gpio  %d\n", config->gpio_pin.pin);
	gpio_set_value_cansleep(config->gpio_pin.pin, 0);
	gpio_free(config->gpio_pin.pin);

	if(config->direction_pin.pin != DONT_CARE){
        	printk(KERN_INFO "Freeing direction gpio  %d\n", config->direction_pin.pin);
       		gpio_set_value_cansleep(config->direction_pin.pin, 0);
		gpio_free(config->direction_pin.pin);
	}
	if(config->function_pin_1.pin != DONT_CARE){
       		printk(KERN_INFO "Freeing function 1 gpio  %d\n", config->function_pin_1.pin);
        	gpio_set_value_cansleep(config->function_pin_1.pin, 0);
		gpio_free(config->function_pin_1.pin);
	}
	if(config->function_pin_2.pin != DONT_CARE){
        	printk(KERN_INFO "Freeing function 1 gpio  %d\n", config->function_pin_2.pin);
        	gpio_set_value_cansleep(config->function_pin_2.pin, 0);
		gpio_free(config->function_pin_2.pin);
	}

	return 1;

}

int set_mux(struct pins* pins, void* device_struct){

    struct hcsr_dev *hcsr_obj = (struct hcsr_dev*)device_struct;

    
    int gpio_trigger_pin, gpio_echo_pin;
    gpio_echo_pin = set_pin_conf(pins->echo_pin,INPUT_FUNC);
    gpio_trigger_pin= set_pin_conf(pins->trigger_pin,OUTPUT_FUNC);

    hcsr_obj->pin.trigger_pin = pins->trigger_pin ;
    hcsr_obj->pin.echo_pin = pins->echo_pin;
    hcsr_obj->pin.gpio_trigger_pin = gpio_trigger_pin ;
    hcsr_obj->pin.gpio_echo_pin = gpio_echo_pin;

    printk(KERN_INFO "Echo Pin  %d and Trigger Pin %d\n",hcsr_obj->pin.echo_pin, hcsr_obj->pin.trigger_pin);
    printk(KERN_INFO "Echo gpio  %d and Trigger gpio %d\n",hcsr_obj->pin.gpio_echo_pin, hcsr_obj->pin.gpio_trigger_pin);

    return 1;

}


static int start_sampling(void *handle)
{
    struct hcsr_dev *hcsr_obj = (struct hcsr_dev*)handle;
    int counter = 0;
    int i,total_distance, distance = 0;
    struct data measurement;
    unsigned long long timestamp;
 	
	if(set_interrupt((void*)hcsr_obj) < 0){
	     printk(KERN_INFO "Error While setting up the interrupt\n");
	     return -1;
    	}

    	clear_buffer(&hcsr_obj->all_samples, hcsr_obj->param.number_of_samples+2, &hcsr_obj->all_samples.sem, &hcsr_obj->all_samples.Lock);

	//printk("entered thread with echo pin %d and trigger pin %d  %d %d\n", hcsr_obj->pin.gpio_echo_pin, hcsr_obj->pin.gpio_trigger_pin, hcsr_obj->param.delta, hcsr_obj->param.number_of_samples);
	do{
        printk("Sample *** %d \n", counter);
		gpio_set_value_cansleep(hcsr_obj->pin.gpio_trigger_pin, 0);
		gpio_set_value_cansleep(hcsr_obj->pin.gpio_trigger_pin, 1);
		udelay(30);
   		gpio_set_value_cansleep(hcsr_obj->pin.gpio_trigger_pin, 0);
		mdelay(hcsr_obj->param.delta);
		counter++;
	}while(!kthread_should_stop() && counter < hcsr_obj->param.number_of_samples + 2);

    total_distance = 0;
    timestamp = get_tsc();
    for(i = 1;i < hcsr_obj->param.number_of_samples+1; i++){
        distance += read_fifo(&hcsr_obj->all_samples, &hcsr_obj->all_samples.sem, &hcsr_obj->all_samples.Lock).distance;
    }

    measurement.timestamp = timestamp;
    measurement.distance = distance/hcsr_obj->param.number_of_samples;
    insert_buffer(&hcsr_obj->buffer, measurement, &hcsr_obj->buffer.sem, &hcsr_obj->buffer.Lock);
    //printk("Calculating distance %d \n", distance/hcsr_obj->param.number_of_samples);
    release_irq((void*)hcsr_obj);
    hcsr_obj->is_in_progress = false;

    return 0;
}

ssize_t start_measurement(struct file *file, const char *buff, size_t size, loff_t *lt){

	struct hcsr_dev *device_data = file->private_data;
	int *input;
	// printk(KERN_INFO "%s starting the sampling\n", device_data->device.name);
	if (!(input= kmalloc(sizeof(int), GFP_KERNEL)))
	{
		printk("Bad Kmalloc\n");
		return -ENOMEM;
	}

	if (copy_from_user(input, buff, sizeof(int))){
		return -EFAULT;
	}
	//spin_lock_irqsave(&device_data->inprogress_lock, flag );
	if(device_data->is_in_progress){
		printk(KERN_INFO "On-Going Measurement\n");
		return -EINVAL;
	}else{
		if(input  > 0) {
                        printk(KERN_INFO "Clearing the buffer\n");
			clear_buffer(&device_data->buffer, MAX_BUFF_SIZE, &device_data->buffer.sem, &device_data->buffer.Lock);
		}

	printk(KERN_INFO "Starting the Sampling now!!!!!\n");	
		device_data->is_in_progress = true;
	//spin_unlock_irqrestore(&device_data->inprogress_lock, flag );
	kthread_run(&start_sampling,(void *)device_data, "start_sampling");

	}

	kfree(input);
	return 1;
}


ssize_t read_buffer(struct file *file, char *buff, size_t size, loff_t *lt) {
	struct hcsr_dev *device_data = file->private_data;
	if(device_data->buffer.count > 0 || device_data->is_in_progress){
		struct data readings = read_fifo(&device_data->buffer, &device_data->buffer.sem, &device_data->buffer.Lock);
		copy_to_user(buff, &readings, sizeof(struct data));
	}else{
	    start_measurement(file, buff, size, lt);
	}
}


int device_open(struct inode *inode, struct file *file){
	int dev_minor= iminor(inode);
	struct hcsr_dev* temp = HEAD;
	
	while(temp != NULL){
	//printk(KERN_INFO "checking device %d\n", temp->device.minor);
		if(temp->device.minor == dev_minor){
		  file->private_data = temp;
		  printk(KERN_INFO "%s device is opened\n", temp->device.name);
		  break;	
		}
		temp = temp->next;
	}
	if(temp == NULL){
		printk(KERN_INFO "Device not found with minor number %d\n", dev_minor);
		return -1;
	}
	return 0;
}


bool validate_pins(struct pins *pin){
	
	if(pin->echo_pin < 0 || pin->trigger_pin < 0){
		return false;
	}
	
	if(!check_pin(pin->echo_pin,INPUT_FUNC)){
		return false;
	}
	
	if(!check_pin(pin->trigger_pin,OUTPUT_FUNC)){
		return false;
	}

	return true;
}
/**

	IOCTL for Seeting pins and configuration of HCSR device

*/
long ioctl_handle(struct file *file, unsigned int command, unsigned long ioctl_param){
	struct pins *pin_conf;
	struct parameters* param;
	struct hcsr_dev *device_data = file->private_data;
	printk(KERN_INFO "INSIDE IOCTL\n");
	switch(command){
		case CONFIG_PINS:
                    printk(KERN_INFO "Setting the Pins\n");
					pin_conf = kmalloc(sizeof(struct pins), GFP_KERNEL);
					copy_from_user(pin_conf, (struct pins*) ioctl_param, sizeof(struct pins));
					if(!validate_pins(pin_conf)){
						printk(KERN_INFO "Invalid Pins!\n");
						return -EINVAL;
					}
                    			set_mux(pin_conf,(void*)device_data);
					kfree(pin_conf);
					break;
		case SET_PARAMETERS:
                    printk(KERN_INFO "Setting the parameters for the %s\n", device_data->device.name);
					param = kmalloc(sizeof(struct parameters), GFP_KERNEL);
					copy_from_user(param, (struct parameters*) ioctl_param, sizeof(struct parameters));
					device_data->param.number_of_samples = param->number_of_samples;
					device_data->param.delta = param->delta;
					    if(alloc_buffer(&device_data->all_samples, device_data->param.number_of_samples + 2) < 0){
						printk(KERN_DEBUG "ERROR while allocating memory\n");
						return -1;
					    }
					kfree(param);
					break;
		default: return -EINVAL;
	}


	return 1;
}

int release(struct inode *inode, struct file *file){
	// release_irq((void*)device_struct);
	printk(KERN_INFO "closing device \n");
	return 0;
}

static struct file_operations fops = {

	.open = device_open,
	.write = start_measurement,
	.read = read_buffer,
	.unlocked_ioctl = ioctl_handle,
	.release = release,
};


static int __init init_gpio_module(void){

    int i, error;
    char *name;
    struct hcsr_dev* device_pointer, *temp;
    HEAD = NULL;

    for(i = 0; i < number_of_device; i++) {
	
	if (!(name = kmalloc(sizeof(char)*20, GFP_KERNEL)))
	{
		printk("Bad Kmalloc\n");
		return -ENOMEM;
	}

	memset(name, 0, sizeof(char)*20);
	snprintf(name, sizeof(char)*20, "%s_%d", DEVICE_NAME_PREFIX, i);
	printk(KERN_INFO "Registering the device %s\n", name);

	if (!(device_pointer = kmalloc(sizeof(struct hcsr_dev), GFP_KERNEL)))
	{
		printk("Bad Kmalloc\n");
		return -ENOMEM;
	}
	
	device_pointer->next = NULL;

	memset(device_pointer, 0, sizeof (struct hcsr_dev));

	device_pointer->device.minor = MISC_DYNAMIC_MINOR;
	device_pointer->device.name = name;
	device_pointer->device.fops = &fops;
	
	error = misc_register(&device_pointer->device);
		if(error){
		printk("Error while registering device\n");
		return -1;
	}
	
	if(HEAD == NULL){
		HEAD = device_pointer;
		temp = HEAD;
	}else{
		temp->next = device_pointer;
		temp = temp->next;
	}
	
	if(alloc_buffer(&device_pointer->buffer, MAX_BUFF_SIZE) < 0){
        	printk(KERN_DEBUG "ERROR while allocating memory\n");
	   	return -1;
    	}
	init_buffer(&device_pointer->buffer,MAX_BUFF_SIZE, &device_pointer->buffer.sem,  &device_pointer->buffer.Lock);
	spin_lock_init(&device_pointer->inprogress_lock);
	printk(KERN_INFO " %s device registered successfully!!\n", name);

    }

	return 0;

}

static void __exit exit_gpio_driver(void){

	printk(KERN_INFO "Removing device\n");
	struct hcsr_dev* temp;
	temp =  HEAD;
	struct hcsr_dev* device_struct;
	
	while(temp != NULL){

		device_struct = temp;
		temp = temp->next;
		mux_free_pins(device_struct->pin.trigger_pin, OUTPUT_FUNC);
		mux_free_pins(device_struct->pin.echo_pin, INPUT_FUNC);
		printk("freed\n");
		misc_deregister(&device_struct->device);
		kfree(device_struct);
		
	}

}

module_init(init_gpio_module);
module_exit(exit_gpio_driver);
MODULE_LICENSE("GPL v2");

