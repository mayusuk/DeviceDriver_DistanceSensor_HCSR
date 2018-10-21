#include <linux/semaphore.h>
#include "data.h"
DEFINE_MUTEX(buffer_lock);

struct buff {
	struct data* result;
	int head,tail;
	int size;
	int count;
    struct semaphore sem;
    spinlock_t Lock;
};

void init_buffer(struct buff* buffer, int size,  struct semaphore *sem, spinlock_t* Lock){
    printk(KERN_INFO "Initialisaing the buffer\n");
	buffer->tail = 0;
	buffer->head = 0;
	buffer->count = 0;
	buffer->size = size;
	sema_init(sem, 0);
	spin_lock_init(Lock);
}


int insert_buffer_without_lock(struct buff * buffer, struct data data){

	buffer->result[buffer->tail++] = data;
	if(buffer->tail >= buffer->size){
		buffer->size = 0;
	}
	return 1;
}

struct data read_fifo_without_lock(struct buff * buffer){

	struct data result = buffer->result[buffer->head];
	buffer->head--;
	if(buffer->head < 0){
		buffer->head = 0;
	}
	return result;
}

int alloc_buffer(struct buff* buffer, int size){
    buffer->result = kmalloc(sizeof(struct data)*size, GFP_KERNEL);
    if(!buffer->result) {
	   printk(KERN_DEBUG "ERROR while allocating memory to buffer\n");
	   return -1;
	}
	return 0;
}


struct data read_fifo(struct buff * buffer,  struct semaphore *sem, spinlock_t* Lock){

    unsigned long flag;
    int res;
	
	spin_lock_irqsave(Lock, flag );
	down(sem);
	struct data result = buffer->result[buffer->head];
	buffer->head++;
	if(buffer->head >= buffer->size){
		buffer->head = 0;
		buffer->count = buffer->tail;
	}
	buffer->count--;
    spin_unlock_irqrestore(Lock, flag);

    printk("Read data from buffer\n");
	return result;
}


int insert_buffer(struct buff * buffer, struct data data,  struct semaphore *sem, spinlock_t* Lock){

    unsigned long flag;
    printk("Adding Data to the buffer\n");
	spin_lock_irqsave(Lock, flag );
	if(buffer->tail >= buffer->size){
        	//printk("Buffer Overflow!! Overwritting the values\n");
		buffer->tail = 0;
	}
	buffer->result[buffer->tail++] = data;
	buffer->count++;
	if(buffer->count > buffer->size) buffer->count--;

    up(sem);
	spin_unlock_irqrestore(Lock, flag);
    //printk("Data Added to the buffer\n");

	return 1;
}

void clear_buffer(struct buff* buffer, int size,  struct semaphore *sem, spinlock_t* Lock) {
	unsigned long flag;	
	spin_lock_irqsave(Lock, flag );
	buffer->tail = 0;
	buffer->head = 0;
	buffer->count = 0;
        memset(buffer->result, 0, sizeof(buffer->result));
	sema_init(sem, 0);
        printk("Buffer is cleared\n");
	spin_unlock_irqrestore(Lock, flag);
}
