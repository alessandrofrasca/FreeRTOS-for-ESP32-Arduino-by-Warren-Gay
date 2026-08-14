/*
In addition to the queue of tasks ready for execution, the scheduler also has access to the blocked list (the suspended list exists for suspended tasks instead, and the main difference between these two different types of list is that the tasks in the suspended list do not return to the ready task queue without user input), a queue of tasks not yet ready to run (perhaps because they are waiting for specific data), therefore avoiding the unnecessary allocation of time slices to tasks that would otherwise remain idle.
Beyond this, FreeRTOS allows us to create data queue: a task can copy data into this new type of queue, which is useful for sharing information between two tasks. While global variables can lead to data corruption and other issues (such as one task reading a variable's value before another has finished writing it), data queues do not come with these problems.
When a data item arrives at the data queue, two theoretical behaviors can occur: balking (the data item does not enter the queue) or reneging (the data item, having already entered, leaves the queue).
What happens if the data queue is full? Two possible scenarios for a common task. The sending task can decide to wait for space to become available (with the scheduler moving it to the blocked list) or to give up and do something else (balking, because the data item will not enter the queue).
ISR Case: if the send operation occurs within an ISR (Interrupt Service Routine, a hardware-managed function triggered by a physical event that temporarily suspends scheduler execution to perform a very quick task), blocking is not an option. An ISR cannot be placed in the blocked list, so giving up immediately (balking) is the only correct choice.
FreeRTOS does not support data reneging; once a data item has entered the queue, it remains there until it is received by a task or the queue is reset.
When we create a data queue, the capacity assigned to it must be fixed: this means that if it fills up, it is not possible to add more space.
In FreeRTOS, the data queue service discipline is FIFO (First In First Out, so the first data item inserted will be the first to be received), with a single exception: if there is important data that needs to skip the queue, we can choose to send it directly to the front of the queue, ahead of all others, violating FIFO.
Multiple tasks can provide data items to the queue, just as multiple tasks can receive them. Each participating task can insert/receive only one item at a time (all of it, never partially).
Just as there are static tasks, static queues exist. 
*/
// Here is an example:
typedef unsigned long qitem_t; // Defining the data type of each item in the queue (typdef is completely optional). We could have chosen bool, char, or unsigned instead of unsigned long. We can choose the item type as we please.
#define QUEUE_DEPTH 10 // Defining queue's maximum amount of items (using a macro is completely optional).

static uint8_t qstorage[QUEUE_DEPTH * sizeof(qitem_t)]; // Allocating static memory to the queue by creating a byte array (corresponding to the queue's storage).
static StaticQueue_t qobj; // A queue's object is useful to manage its states and providing priority-based ordering for tasks.

QueueHandle_t qh = xQueueCreateStatic(
  QUEUE_DEPTH,
  sizeof(qitem_t), // Byte size of each item.
  &qstorage[0], // Address of the first element of the storage.
  &qobj
);