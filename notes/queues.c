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

/*
Factory Hydraulic Press Demonstration.
Note: this example differs from the previous one since the operator must hold down both buttons for the press (the LED) to function. If even one button is released, a safety measure must be implemented: the press must be immediately disabled.
*/
#define GPIO_LED 12
#define GPIO_BUTTONL 25
#define GPIO_BUTTONR 26

static QueueHandle_t qh;

// Button debouncing task:

static void debounce_task(void* argp){
  unsigned button_gpio = *(unsigned*)argp;
  uint32_t state = 0, level, mask = 0x7FFFFFFF;
  int event, last = -999;

  for(;;){
    level = !digitalRead(button_gpio); // When the input is LOW (i.e., the button is pressed), we get 1; when it is HIGH (i.e., the button is not pressed), we get 0.
    state = (state << 1) | level;
    if ((state & mask) == mask){ // The button is being pressed for real.
      event = button_gpio;
    } else event = -button_gpio;
    
    if (event != last){
      if (xQueueSendToBack(qh, &event, 1) == pdPASS){
        last = event;
      }
    }
    taskYIELD();
  }
}

// Hydraulic press task (LED):

static void press_task(void* argp){
  digitalWrite(GPIO_LED, LOW); // Making sure the press (LED) is off at the beginning.
  static const uint32_t enable = (1 << GPIO_BUTTONL) | (1 << GPIO_BUTTONR); // 'const' declares that the variable's value cannot be modified after the variable has been defined. It is used to protect the variable from accidental changes.
  // Depending on the selected GPIOs, 'enable` will be the following bitmask: '0000 0110 0000 0000 0000 0000 0000 0000'.
  BaseType_t rc;
  int event;
  uint32_t state = 0;

  for(;;){ 
    rc = xQueueReceive(qh, &event, portMAX_DELAY);
    assert(rc == pdPASS);

    if (event >= 0){
      // The button is being pressed.
      state |= 1 << event;
    }
    if (event < 0){
      // The button is not being pressed.
      state &= ~(1 << -event); // '~' is the bitwise NOT operator: it inverts all the bits.
    }
    /*
    After these two if statements, the `state` variable will hold a bitmask containing the current states of the buttons at the 25th and 26th bits.
    If state = enable, the press will be activated. In all other cases, it will not.
    */ 
    if (state == enable){
      digitalWrite(GPIO_LED, HIGH);
    } else digitalWrite(GPIO_LED, LOW);
  }
}

void setup(){
  int app_cpu = xPortGetCoreID();
  static int left = GPIO_BUTTONL;
  static int right = GPIO_BUTTONR;
  
  TaskHandle_t press_taskh;
  TaskHandle_t debounceL_taskh;
  TaskHandle_t debounceR_taskh;
  BaseType_t rc;

  delay(2000); // Allowing USB to connect properly.
  qh = xQueueCreate(40, sizeof(int));
  assert(qh); // If qh is empty, then abort().

  pinMode(GPIO_LED, OUTPUT);
  pinMode(GPIO_BUTTONL, INPUT_PULLUP);
  pinMode(GPIO_BUTTONR, INPUT_PULLUP);

  rc = xTaskCreatePinnedToCore(
    debounce_task,
    "debounceL",
    2048,
    &left,
    1,
    &debounceL_taskh,
    app_cpu
  );
  assert(rc == pdPASS);
  assert(debounceL_taskh);

  rc = xTaskCreatePinnedToCore(
    debounce_task,
    "debounceR",
    2048,
    &right,
    1,
    &debounceR_taskh,
    app_cpu
  );
  assert(rc == pdPASS);
  assert(debounceR_taskh);

  rc = xTaskCreatePinnedToCore(
    press_task,
    "led",
    2048,
    nullptr,
    1,
    &press_taskh,
    app_cpu
  );
  assert(rc == pdPASS);
  assert(press_taskh);
}

void loop(){
  // Not used.
  vTaskDelete(nullptr);
}

// However, there is a safety issue: if xQueueSendToBack(qh, &event, 1) were to fail within debounce_task while event < 0 (meaning we were attempting to queue the command to stop the press), it would not be possible to stop the press.
// In order to implement a safety measure:

#include <stdio.h>
#define GPIO_LED 12
#define GPIO_BUTTONL 25
#define GPIO_BUTTONR 26

static QueueHandle_t qh;
static const int reset = -998; // This is a special value that will be used to reset the press (LED) in case of a safety issue.

static void debounce_task(void* argp){
  unsigned button_gpio = *(unsigned*)argp;
  uint32_t state = 0, level, mask = 0x7FFFFFFF;
  int event, last = -999;

  for(;;){
    level = !digitalRead(button_gpio); 
    if ((state & mask) == mask){ 
      event = button_gpio;
    } else event = -button_gpio;
    
    if (event != last){
      if (xQueueSendToBack(qh, &event, 0) == pdPASS){ // Waiting ticks are set to 0, meaning that if the queue is full, the function will return immediately. If we kept 1, the function would wait for 1 tick before returning, which is not acceptable in this case.
        last = event;
      } else if (event < 0){ // If the queue is full (xQueueSendToBack()!=pdPASS) and we are trying to send a command to stop the press (event < 0)...
        do {xQueueReset(qh);} while (xQueueSendToBack(qh, &reset, 0) != pdPASS); // ...we will reset the queue and, once done, we will send the global variable 'reset' to the queue, which will be used to stop the press (LED) in case of a safety issue.
        last = event;
      }
    }
    taskYIELD();
  }
}

static void press_task(void* argp){
  digitalWrite(GPIO_LED, LOW); 
  static const uint32_t enable = (1 << GPIO_BUTTONL) | (1 << GPIO_BUTTONR); 
  
  BaseType_t rc;
  int event;
  uint32_t state = 0;

  for(;;){ 
    rc = xQueueReceive(qh, &event, portMAX_DELAY);
    assert(rc == pdPASS);

    if (event == reset){
        state = 0; // Resetting the state variable to ensure the press (LED) is turned off.
        digitalWrite(GPIO_LED, LOW);
        printf("RESETTING!\n");
        continue; // This will skip the rest of the (;;) loop and go back to waiting for the next event.
    }
    if (event >= 0){
      state |= 1 << event;
    }
    if (event < 0){
      state &= ~(1 << -event); 
    }

    if (state == enable){
      digitalWrite(GPIO_LED, HIGH);
    } else digitalWrite(GPIO_LED, LOW);
  }
}

void setup(){
  int app_cpu = xPortGetCoreID();
  static int left = GPIO_BUTTONL;
  static int right = GPIO_BUTTONR;
  
  TaskHandle_t press_taskh;
  TaskHandle_t debounceL_taskh;
  TaskHandle_t debounceR_taskh;
  BaseType_t rc;

  delay(2000); 
  qh = xQueueCreate(2, sizeof(int));
  assert(qh);

  pinMode(GPIO_LED, OUTPUT);
  pinMode(GPIO_BUTTONL, INPUT_PULLUP);
  pinMode(GPIO_BUTTONR, INPUT_PULLUP);

  rc = xTaskCreatePinnedToCore(
    debounce_task,
    "debounceL",
    2048,
    &left,
    1,
    &debounceL_taskh,
    app_cpu
  );
  assert(rc == pdPASS);
  assert(debounceL_taskh);

  rc = xTaskCreatePinnedToCore(
    debounce_task,
    "debounceR",
    2048,
    &right,
    1,
    &debounceR_taskh,
    app_cpu
  );
  assert(rc == pdPASS);
  assert(debounceR_taskh);

  rc = xTaskCreatePinnedToCore(
    press_task,
    "led",
    2048,
    nullptr,
    1,
    &press_taskh,
    app_cpu
  );
  assert(rc == pdPASS);
  assert(press_taskh);
}

void loop(){
  vTaskDelete(nullptr);
}

// Other functions that may prove useful, even though they often indicate that the program has not been well structured:
uxQueueMessagesWaiting(qh) // Returns the number of items currently in the queue.
uxQueueSpacesAvailable(qh) // Returns the number of spaces available in the queue, which is similar to the previous function.
xQueuePeek(qh, &item, wait_ticks) // Returns the item at the front of the queue without removing it. The last argument is the number of ticks to wait for an item to become available if the queue is empty. In this case, it is set to 0, meaning that if the queue is empty, the function will return immediately.

// We may sometimes need to use variable-length data items (for example, we might want to exchange strings between two tasks: we cannot simply use `xQueueSendToBack(qh, string, wait_ticks)` because strings have varying lengths that might not match the fixed data item size defined for the queue). In this case, it is best to use pointers as the queue's data items.
static void send_task(void* argp){
  char* str = strdup("Hello, world!"); // This function allocates heap memory for the string and copies it into that memory. The pointer to the allocated memory is returned. 'str' will disappear when the task is done, but the string will remain in the queue. This is basically the whole memory block containing the string, which is what we want to send to the queue.
  xQueueSendToBack(qh, &str, portMAX_DELAY); // We send the pointer to the string to the queue.
} // Once the task is done, str gets eliminated, but the string's address remains in the queue. The receiving task must free the memory after processing the string, or we would have a memory leak.
// If we typed 'xQueueReset(qh)' at the end of the task's function, the string would be lost and we would have a memory leak (since we couldn't free the memory).
// If we typed 'free(str)' at the end of the task's function, we would free the memory before the receiving task could process it, which would lead to undefined behavior (since the receiving task would be trying to access memory that has already been freed).
static void receive_task(void* argp){
  char* str;
  xQueueReceive(qh, &str, portMAX_DELAY); // We receive the pointer to the string from the queue.
  printf("%s\n", str); // We print the string.
  free(str); // We free the memory allocated for the string.
} 
// It is possible to work with an ISR (Interrupt Service Routine) and a queue, but specific syntax must be used.