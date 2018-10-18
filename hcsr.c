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

#include "hcsr.h"
#include "buffer.h"
#include "muxtable.h"

#define MAX_BUFF_SIZE  5
#define DEVICE_NAME_PREFIX "hcsr"
#define CLASS_NAME "HCSR_DRV"




int number_of_device = 1;

module_param(number_of_device, int, S_IRUSR);

struct hcsr_dev {
    struct miscdevice device;
	struct pins pin;
	struct parameters param;
	struct buff buffer;
	bool is_in_progress;
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


static struct hcsr_dev *device_struct;
struct buff *all_samples;
unsigned long long  tsc_raising, tsc_falling;
static irq_handler_t interrupt_handler(unsigned int irq, void *dev_id) {

    printk(KERN_INFO"INTERRUPT HANDLER\n");
	struct hcsr_dev *hcsr_obj = (struct hcsr_dev*)dev_id;
	int distance;
	struct 	data measurement;
	int check = gpio_get_value(hcsr_obj->pin.gpio_echo_pin);

	if (irq == gpio_to_irq(hcsr_obj->pin.echo_pin))
	{
		if (check ==0)
		{
            printk("gpio pin is low\n");
	 		tsc_falling=get_tsc();
			distance =  ((int)(tsc_falling - tsc_raising)/(139200));
			measurement.timestamp = tsc_falling;
			measurement.distance = distance;
			printk(KERN_INFO "***Distance *** %d",distance);
			insert_buffer(all_samples, measurement, &all_samples->sem, &all_samples->Lock);
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


int set_interrupt(void *device_struct){

	struct hcsr_dev *hcsr_obj = (struct hcsr_dev*)device_struct;
    int irqNumber, result;
    irqNumber = gpio_to_irq(hcsr_obj->pin.gpio_echo_pin);
	if(irqNumber < 0){
		printk("GPIO to IRQ mapping failure \n" );
      		return -1;
	}
    printk(KERN_INFO "IRQNUMBER is %d", irqNumber);

	result = request_irq(irqNumber,               	// The interrupt number requested
		 (irq_handler_t) interrupt_handler, 	    // The pointer to the handler function (above)
		 IRQF_TRIGGER_RISING,                 		// Interrupt is on rising edge
		 "ebb_gpio_handler",                  		// Used in /proc/interrupts to identify the owner
		 (void*)device_struct);                     // The *dev_id for shared interrupt lines, NULL here

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
        printk(KERN_INFO "Setting gpio %d as input", config->gpio_pin.pin);
        gpio_request(config->gpio_pin.pin, "Input");
		gpio_direction_input(config->gpio_pin.pin);
	}
	if(config->direction_pin.pin != DONT_CARE){
        printk(KERN_INFO "Setting direction gpio  %d", config->direction_pin.pin);
        gpio_request(config->direction_pin.pin, "Direction");
		gpio_direction_output(config->direction_pin.pin, config->direction_pin.level);
	}
	if(config->function_pin_1.pin != DONT_CARE){
        printk(KERN_INFO "Setting function 1 gpio  %d", config->function_pin_1.pin);
        gpio_request(config->function_pin_1.pin, "function_1");
		gpio_direction_output(config->function_pin_1.pin, config->function_pin_1.level);
	}
	if(config->function_pin_2.pin != DONT_CARE){
        printk(KERN_INFO "Setting function 1 gpio  %d", config->function_pin_2.pin);
        gpio_request(config->function_pin_2.pin, "function_2");
		gpio_direction_output(config->function_pin_2.pin, config->function_pin_2.level);
	}

	return 	config->gpio_pin.pin;

}

int set_mux(struct pins* pins, void* device_struct){
	//IO-0 trigger pin
	// IO -1 echo pin
    struct hcsr_dev *hcsr_obj = (struct hcsr_dev*)device_struct;
    /*
	printk(KERN_INFO "Setting up the pins of mux");
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
	gpio_direction_output(45, 0);

    hcsr_obj->pin.trigger_pin = 11;
    hcsr_obj->pin.echo_pin = 12;

	return 1;
    */
    printk(KERN_INFO "Echo Pin  %d *** Trigger Pin %d\n",pins->echo_pin, pins->trigger_pin);

	int gpio_trigger_pin, gpio_echo_pin;
	gpio_echo_pin = set_pin_conf(pins->echo_pin,INPUT_FUNC);
	gpio_trigger_pin= set_pin_conf(pins->trigger_pin,OUTPUT_FUNC);

    hcsr_obj->pin.trigger_pin = pins->trigger_pin ;
    hcsr_obj->pin.echo_pin = pins->echo_pin;
    hcsr_obj->pin.gpio_trigger_pin = gpio_trigger_pin ;
    hcsr_obj->pin.gpio_echo_pin = gpio_echo_pin;

	printk(KERN_INFO "Echo Pin  %d *** Trigger Pin %d\n",hcsr_obj->pin.gpio_echo_pin, hcsr_obj->pin.gpio_trigger_pin);
	return 1;

}


static int start_sampling(void *handle)
{
	struct hcsr_dev *hcsr_obj = (struct hcsr_dev*)handle;
	int counter = 0;
    int i,total_distance, distance = 0;
	struct 	data measurement;
    unsigned long long timestamp;
    if(set_interrupt((void*)hcsr_obj) < 0){
             printk(KERN_INFO "Error While setting up the interrupt\n");
             return -1;
    }

    init_buffer(all_samples, hcsr_obj->param.number_of_samples+2, &all_samples->sem, &all_samples->Lock);

	printk("entered thread\n");
	do{
        printk("entered thread %d \n", counter);
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
        distance += read_fifo(all_samples, &all_samples->sem, &all_samples->Lock).distance;
        printk("Calculating distance %d \n", distance);
    }

    measurement.timestamp = timestamp;
    measurement.distance = total_distance/hcsr_obj->param.number_of_samples;
    insert_buffer(&hcsr_obj->buffer, measurement, &hcsr_obj->buffer.sem, &hcsr_obj->buffer.Lock);

	hcsr_obj->is_in_progress = false;
	release_irq((void*)hcsr_obj);

	return 0;
}

ssize_t start_measurement(struct file *file, const char *buff, size_t size, loff_t *lt){

	struct hcsr_dev *device_data = file->private_data;
	int *input;

	printk(KERN_INFO "%s starting the sampling\n", device_data->device.name);
	if (!(input= kmalloc(sizeof(int), GFP_KERNEL)))
	{
		printk("Bad Kmalloc\n");
		return -ENOMEM;
	}

	if (copy_from_user(input, buff, sizeof(int))){
		return -EFAULT;
	}
	if(device_data->is_in_progress){
		printk(KERN_INFO "On-Going Measurement");
		return -EINVAL;
	}else{
		if(input  == 0) {
            printk(KERN_INFO "Clearing the buffer\n");
			// write code to handle clearing of buffer
		}
		printk(KERN_INFO "Starting the Sampling now!!!!!\n");
		device_data->is_in_progress = true;
        kthread_run(&start_sampling,(void *)device_data, "start_sampling");

	}
	kfree(input);
	return 1;
}


ssize_t read_buffer(struct file *file, char *buff, size_t size, loff_t *lt) {
	struct hcsr_dev *device_data = file->private_data;
	if(device_data->is_in_progress){
		struct data readings = read_fifo(&device_data->buffer, &device_data->buffer.sem, &device_data->buffer.Lock);
		printk(KERN_INFO "READING from buffer %d", readings.distance);
	}else{
	    start_measurement(file, buff, size, lt);
	}
}


int device_open(struct inode *inode, struct file *file){
	//struct hcsr_dev *hcsr_devp = NULL;
	//struct miscdevice *misc_dev = NULL;
	//misc_dev = container_of(inode->i_fop, struct miscdevice, fops);
	//hcsr_devp = container_of(misc_dev, struct hcsr_dev, device);
	file->private_data = device_struct;
	printk(KERN_INFO "%s device is opened\n", device_struct->device.name);
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
	struct parameters* param;
	struct hcsr_dev *device_data = file->private_data;
	printk(KERN_INFO "INSIDE IOCTL\n");
	switch(command){
		case CONFIG_PINS:
                    printk(KERN_INFO "Setting the Pins\n");
					pin_conf = kmalloc(sizeof(struct pins), GFP_KERNEL);
					/*if(!validate_pins(pin_conf)){
						return -EINVAL;
					}*/
					copy_from_user(pin_conf, (struct pins*) ioctl_param, sizeof(struct pins));
                    set_mux(pin_conf,(void*)device_data);
					kfree(pin_conf);
					break;
		case SET_PARAMETERS:
                    printk(KERN_INFO "Setting the parameters for the %s\n", device_data->device.name);
					param = kmalloc(sizeof(struct parameters), GFP_KERNEL);
					copy_from_user(param, (struct parameters*) ioctl_param, sizeof(param));
					device_data->param.number_of_samples = param->number_of_samples;
					device_data->param.delta = param->delta;
                    if(alloc_buffer(all_samples, device_data->param.number_of_samples + 2) < 0){
                        printk(KERN_DEBUG "ERROR while allocating memory");
                        return -1;
                    }
					kfree(param);
					break;
		default: return -EINVAL;
	}


	return 1;
}

int release(struct inode *inode, struct file *file){
	struct hcsr_dev *device_data = file->private_data;
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
	char name[20];
	// table.array_struct = (struct hcsr_dev**)kmalloc(sizeof(struct hcsr_dev *)*number_of_device, GFP_KERNEL);
	// table.array_device_number = kmalloc(sizeof(dev_t)*number_of_devices, GFP_KERNEL);
	//hcsr_class = class_create(THIS_MODULE, CLASS_NAME);
	//snprintf(name, sizeof name, "%s",DEVICE_NAME_PREFIX);
    printk(KERN_INFO "Registering HCSR device");
	device_struct = kmalloc(sizeof(struct hcsr_dev), GFP_KERNEL);
	if(!device_struct) {
	   printk(KERN_DEBUG "ERROR while allocating memory");
	   return -1;
	}
    memset(device_struct, 0, sizeof (struct hcsr_dev));
	device_struct->device.minor = MISC_DYNAMIC_MINOR;
	device_struct->device.name = "HCSR_DEVICE";
	device_struct->device.fops = &fops;

	error = misc_register(&device_struct->device);
	if(error){
        printk("Error while registering device");
        return -1;
	}
    all_samples = kmalloc(sizeof(struct buff), GFP_KERNEL);
    if(alloc_buffer(&device_struct->buffer, MAX_BUFF_SIZE) < 0){
        printk(KERN_DEBUG "ERROR while allocating memory");
	   return -1;
    }
	init_buffer(&device_struct->buffer,MAX_BUFF_SIZE, &device_struct->buffer.sem,  &device_struct->buffer.Lock);
	printk(KERN_INFO "Driver %s is initialised\n", CLASS_NAME);
	return 0;

}

static void __exit exit_gpio_driver(void){

	printk(KERN_INFO "Removing device\n");
	gpio_set_value_cansleep(device_struct->pin.gpio_trigger_pin, 0);
	gpio_set_value_cansleep(device_struct->pin.gpio_echo_pin, 0);
	gpio_free(device_struct->pin.gpio_echo_pin);
    gpio_free(device_struct->pin.gpio_trigger_pin);
	printk("freed\n");
	misc_deregister(&device_struct->device);
	kfree(device_struct);
}

module_init(init_gpio_module);
module_exit(exit_gpio_driver);
MODULE_LICENSE("GPL v2");

