DEFINE_MUTEX(buffer_lock);


struct data {
	unsigned long long timestamp;
	double distance;
}
struct buff {
	struct data* result;
	int head,tail;
	int size;
};

void init_buffer(struct buff* buffer, int size,  struct semaphore *sem){
	buffer->result = kmalloc(sizeof(struct data)*size, GFP_KERNEL);
	buffer->tail = 0;
	buffer->head = 0;
	buffer->size = size;
 	sem = kmalloc(sizeof(struct semaphore), GFP_KERNEL);
	sema_init(sem, 0);
}

struct data read_buffer(struct buff * buffer,  struct semaphore *sem){
		
	__down_interruptible(sem);
	mutex_lock(&buffer_lock);
	struct data result = buffer->result[head];
	buffer->head--;
	if(buffer->head < 0){
		buffer->head = 0;	
	}
	mutex_unlock(&buffer_lock);
	return result;	
}

int insert_buffer(struct buff * buffer, struct data data,  struct semaphore *sem){
		
	mutex_lock(&buffer_lock);
	buffer->result[tail++] = data;
	if(buffer->tail >= buffer->size){
		buffer->size = 0;	
	}
	mutex_unlock(&buffer_lock);
	__up_interruptible(sem);
	return 1;	
}

void clear_buffer(struct buff * buffer, struct semaphore *sem) {
	mutex_lock(&buffer_lock);
	buffer->tail = 0;
	buffer->head = 0;
	sema_init(sem, 0);
	mutex_unlock(&buffer_lock);
}
