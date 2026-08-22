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
If multiple tasks are waiting to insert a data item into the same queue (assuming `wait_ticks` is greater than 0), the task with the highest priority will be the first to be blocked and subsequently insert the data item.
Just as there are static tasks, static queues exist. 
*/
// Here is an example:
typedef unsigned long qitem_t; // Defining the data type of each item in the queue (typdef is completely optional). We could have chosen bool, char, or unsigned instead of unsigned long. We can choose the item type as we please.
#define QUEUE_DEPTH 10 // Defining queue's maximum amount of items (using a macro is completely optional).

static uint8_t qstorage[QUEUE_DEPTH * sizeof(qitem_t)]; // Allocating static memory to the queue by creating a byte array (corresponding to the queue's storage).
static StaticQueue_t qobj; // A queue's object is useful to manage its states and providing priority-based ordering for tasks. It is like a task's TCB: it provides information about the queue.

QueueHandle_t qh = xQueueCreateStatic(
  QUEUE_DEPTH,
  sizeof(qitem_t), // Byte size of each item.
  &qstorage[0], // Address of the first element of the storage.
  &qobj
);

qitem_t my_item = 42;
TickType_t wait_ticks = 2;
BaseType_t rc = xQueueSendToBack( // 'rc' stands for returned code. Its value might be 'errQUEUE_FULL' if the queue is full (and the item could not be placed), otherwise 'pdPASS'. 'xQueueSend()' is equivalent to 'xQueueSendToBack()'.
  qh, // Queue handle to indicate which queue to add the data item to.
  &my_item,
  wait_ticks // If the number of ticks (wait_ticks) is greater than 0 and the queue is full, the task moves to the blocked list for that specific number of ticks or until space opens up in the queue). If the queue is still full (and the item could not be placed), rc equals to 'errQUEUE_FULL'. The special macro portMAX_DELAY (as value of this parameter) can be used to block a task forever if the queue is full.
);
// If we want to send the data item to the beginning of the queue, we can use the xQueueSendToFront() function instead. Receving an item is as easy as adding items:
BaseType_t rc = xQueueReceive( // rc == pdPASS or rc == errQUEUE_EMPTY.
  qh, // Queue handle to receive from.
  &item, // Pointer to data item (it indicates where to copy the data item value, as a local variable in the receiving task). Receiving always occurs from the front of the (FIFO) data queue; there is no receiving from the rear.
  wait_ticks // If the queue is empty, the task is blocked for a certain number of ticks.
);
// Creating a dynamic queue is easier, since FreeRTOS handles the memory allocation.
QueueHandle_t qh = xQueueCreate( // Memory for storage and the queue object is allocated from the heap.
  QUEUE_DEPTH,
  sizeof(qitem_t)
); // If the heap is exhausted or too fragmented, qh == NULL or qh == nullptr. To delete a queue (dynamic or static):
vQueueDelete(qh); // Note: within FreeRTOS, a queue must not be deleted if there are tasks blocked on that queue (whether because the queue is full or because it is empty).
// It is also possible to reset a queue (to empty it). This function is always available, but it does not guarantee that no tasks will remain blocked on the queue as a result.
xQueueReset(qh);
// The following demonstration simulates a car's traction control system, but by illuminating an LED.
#define GPIO_LED 12
#define GPIO_BUTTON 25

static QueueHandle_t qh;

// Button debounce's task:
static void debounce_task(void* argp){
  /*
  Note: a bitmask is used to directly represent bits. Following '0x', there are 8 digits or letters: each represents 4 bits. 7 = '0111', 0 = '0000', F = '1111' (for instance, '0x7FFFFFFF' stands for '0111 1111 1111 1111 1111 1111 1111 1111').
  Relevant note: & and | are bitwise operators (for example, 2 & 3 is interpreted as 00000010 & 00000011 = 00000010, which is 2), while && and || are logical operators (2 && 3 = 1 because neither number is 0, and the same applies to 2 || 3). If !(predicate) means (predicate)==0, then !!(predicate) means ((predicate)==0)==0.
  2 ^ 5 (where ^ is the bitwise XOR operator) means 00000010 ^ 00000101 and returns 00000111 (it checks, bit by bit, which ones are different, returning 1, and which ones are the same, returning 0). << and >> are the left-shift and right-shift bitwise operators, respectively. The 'new bits' that appear as a result of the shifts are 0 (for example: 7 << 2 means shifting 00000111 by 2 bits to the left, resulting in 00011100, which is 28).
  The variable 'level' is going to memorize the value read from the button pin at that instant (it can only be 0 or 1).
  */
  uint32_t level, state = 0, last = 0xFFFFFFFF, mask = 0x7FFFFFFF;
  bool event;
  for (;;){
    level = !!digitalRead(GPIO_BUTTON); // Depending on the libraries used, GPIO-related functions do not necessarily return 1 or 0. Typically, they return a non-zero value for HIGH and zero for LOW. For instance, if the value were 256, the expression ((256==0)==0) would become 0==0, which in turn evaluates to 1.
    state = (state << 1) | level; // On the first iteration, if we have pressed the button, we will get 1; otherwise, 0. The 'state' variable collects the history of the values ​​of 'levels' (up to 32 values).
    if ((state & mask) == mask || (state & mask) == 0){ 
    /*
    When we press a button, the internal metal contacts bounce a hundred times within a few milliseconds, resulting in numerous fluctuations between 0 and 1 (LOW and HIGH). The `if` condition is set to ensure that the signal has remained stable.
    First predicate: if the state has at least one 0 and 31 consecutive 1s, or 32 consecutive 1s.
    Second predicate: if the state has a 1 and 31 consecutive 0s, or 32 consecutive 0s.
    */  
      if (level != last){ // It is useful to prevent situations where the button remains pressed or unpressed for a long time. If we hold down the button, level=last (1=1). If we do not hold it down, level=last (0=0). In both cases, the process proceeds to yielding the time slice.
        event = !!level; // `event = level` would have been enough.
        if (xQueueSendToBack(qh, &event, 1) == pdPASS){ 
          last = level;
        }
      }
    }
    taskYIELD();
  }
}

static void led_task(void* agrp){
  bool led = false, event;
  BaseType_t rc;
  digitalWrite(GPIO_LED, led); // At the beginning, the LED is off.
  for (;;){
    rc = xQueueReceive(qh, &event, portMAX_DELAY);
    assert(rc == pdPASS); // If the returned code is not equal to 'pdPASS', it is time to abort the program.
    if (!event){ //
      led ^= true; // Which means led = led ^ 1, therefore if we press the button (event=0) and the led is already on, it gets turned off.
      digitalWrite(GPIO_LED, led);
    }
  }
}

void setup() {
  int app_cpu = xPortGetCoreID();
  BaseType_t rc;
  TaskHandle_t debounce_taskh;
  TaskHandle_t led_taskh;

  delay(2000); // We are allowing USB to connect.
  qh = xQueueCreate(40, sizeof(bool));
  assert(qh);

  pinMode(GPIO_LED, OUTPUT);
  pinMode(GPIO_BUTTON, INPUT_PULLUP); // The button has a pull-up resistor; consequently, the pin reads HIGH (1 or true) when we are not pressing the button, and LOW (0 or false) when we press it.

  rc = xTaskCreatePinnedToCore(
    debounce_task,
    "debounce",
    2048,
    nullptr,
    1,
    &debounce_taskh,
    app_cpu
  );
  assert(rc==pdPASS);
  assert(debounce_taskh); // `debounce_task == true` evaluates to true for any value of `debounce_task` other than 0 or NULL. If it is equal to 0 (and 0 corresponds to NULL), then the assertion condition is violated.

  rc = xTaskCreatePinnedToCore(
    led_task,
    "led",
    2048,
    nullptr,
    1,
    &led_taskh,
    app_cpu
  );
  assert(rc==pdPASS);
  assert(led_taskh);
}

void loop(){
  vTaskDelete(nullptr); // Because we are not using loopTask.
}