# include <thread.h>


//////////////////////////// STRUCTS AND GLOBALS ////////////////////////////


struct Node {
    void* task;
    struct Node* next_task;
};

struct Thread_Node {
    cnd_t cond_var;
    void* assigned_task;
    struct Thread_Node* next;
};

struct Queue {
    // task queque
    struct Node* head_task;
    struct Node* tail_task;
    // waiting threads queue
    struct Thread_Node* head_thread;
    struct Thread_Node* tail_thread;
    atomic_size_t visited_count;
    mtx_t mutex;
};

static struct Queue tasks_FIFO;

//////////////////////////// QUEUE FUNCTIONS ////////////////////////////


// called before the queue is used. Initialize data structures.
// assumption: is called by a single thread before any concurrent queue operations begin.
// no need to be thread-safe.
void initQueue(void)
{
// assumption 1: malloc cant fail
// assumption 2: no errors in calls to mtx_* and cnd_* functions.

// Initialize task queue data structure
    tasks_FIFO.head_task = NULL;
    tasks_FIFO.tail_task = NULL;
// Initialize threads queue data structure
    tasks_FIFO.head_thread = NULL;
    tasks_FIFO.tail_thread = NULL;


    atomic_init(&tasks_FIFO.visited_count, 0); /// size_t is not a primitive type, initialize visited count to 0
    mtx_init(&tasks_FIFO.mutex, mtx_plain);
}

//------------------------------------------------------------------------------------------

//  cleanup when the queue is no longer needed.
// assumption: no running threads are using the queue when this is called.
// no sleeping threads are waiting on the queue when this is called.
// queue is empty when this is called.
void destroyQueue(void){
// assumption 1: no errors in calls to mtx_* and cnd_* functions.

    mtx_destroy(&tasks_FIFO.mutex);


}


//------------------------------------------------------------------------------------------

// Adds a task to the queue.
// must be thread-safe.
// receives a pointer to an untyped item to be added to the queue.
void enqueue(void* task)
{

// assumption 1: malloc cant fail
// assumption 2: no errors in calls to mtx_* and cnd_* functions.


// lock the queue mutex
// only the thread that called enqueue can modify the queue at this point
    mtx_lock(&tasks_FIFO.mutex);


    // option one: there are waiting threads
    // give the task to the first waiting thread

    if (tasks_FIFO.head_thread != NULL) 
    {
        // get the first waiting thread
        struct Thread_Node* first_waiting_thread = tasks_FIFO.head_thread;
        first_waiting_thread->assigned_task = task;

        // remove the waiting thread from the waiting threads queue
        tasks_FIFO.head_thread = first_waiting_thread->next;
        if (tasks_FIFO.head_thread == NULL) 
        {
            tasks_FIFO.tail_thread = NULL;
        }

        // signal the waiting thread to wake up
        cnd_signal(&first_waiting_thread->cond_var);
        // clean up will be done by the dequeuing thread
    }
    else
    {
    // option two: no waiting threads
    // add the task to the task queue

    // create a new node for the task
    struct Node* new_task = malloc (sizeof (struct Node));
    new_task->task = task;
    new_task->next_task = NULL;

    // add the new task to the task queue
    if (tasks_FIFO.tail_task == NULL) 
    { // queue is empty
        tasks_FIFO.head_task = new_task;
        tasks_FIFO.tail_task = new_task;
    } else {
        tasks_FIFO.tail_task->next_task = new_task;
        tasks_FIFO.tail_task = new_task;
    }

    }
    
    mtx_unlock(&tasks_FIFO.mutex);

}

//------------------------------------------------------------------------------------------


// Remove an item from the queue. Blocks if empty.
// must be thread-safe.
void* dequeue(void)
{
// assumption 1: no errors in calls to mtx_* and cnd_* functions.

    void* task = NULL;
        
    // option 1: there is a task in the queue
    // give it to the calling thread

    mtx_lock(&tasks_FIFO.mutex);

    if (tasks_FIFO.head_task != NULL) 
    {
        // get the first task from the task queue
        struct Node* first_task = tasks_FIFO.head_task;
        task = first_task->task;

        // remove the task from the task queue
        tasks_FIFO.head_task = first_task->next_task;
        // if the queue is empty after removing the task, update the tail pointer
        if (tasks_FIFO.head_task == NULL) 
        {
            tasks_FIFO.tail_task = NULL;
        }

        // free the memory allocated for the task node
        free(first_task);

        // increment visited count
        atomic_fetch_add(&tasks_FIFO.visited_count, 1);

       
    }

    // option 2: no tasks in the queue, the calling thread enters the waiting threads queue
    
    else 
    {
        // create a new thread node for the waiting thread
        struct Thread_Node* new_thread = malloc (sizeof (struct Thread_Node));
        new_thread->assigned_task = NULL;
        new_thread->next = NULL;
        cnd_init(&new_thread->cond_var);

        // add the new thread to the waiting threads queue
        if (tasks_FIFO.tail_thread == NULL) 
        { // queue is empty
            tasks_FIFO.head_thread = new_thread;
            tasks_FIFO.tail_thread = new_thread;
        } else {
            tasks_FIFO.tail_thread->next = new_thread;
            tasks_FIFO.tail_thread = new_thread;
        }

        // wait for a task to be assigned
        while (new_thread->assigned_task == NULL) 
        {
            cnd_wait(&new_thread->cond_var, &tasks_FIFO.mutex);
        }

        // get the assigned task
        task = new_thread->assigned_task;

        // free the memory allocated for the thread node
        cnd_destroy(&new_thread->cond_var);
        free(new_thread);

        // increment visited count
        atomic_fetch_add(&tasks_FIFO.visited_count, 1);
        
    }

    mtx_unlock(&tasks_FIFO.mutex);
    return task;
}



//------------------------------------------------------------------------------------------


// Returns the total count of items that have been both enqueued and subsequently dequeued. 
// should not block due to concurrent operations, yet must return accurate results if no concurrent
// operations are occurring.
// must be thread-safe.
size_t visited(void)
{
    // atomic variable to keep track of visited count
    return atomic_load(&tasks_FIFO.visited_count);
}
