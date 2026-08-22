#define GPIO_LED 12
#define GPIO_BUTTON 25

static QueueHandle_t qh;

static void debounce_task(void* argp) {
  uint32_t level, state = 0, mask = 0x7FFFFFFF, last = 0xFFFFFFFF;
  bool event;
  for (;;){
    level = !!digitalRead(GPIO_BUTTON);
    state = (state << 1) | level;
    if ((state & mask) == mask || (state & mask) == 0){
      if (level != last){
        event = !!level; // Pointless, event = level would have been enough.
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
  digitalWrite(GPIO_LED, led); 
  for (;;){
    rc = xQueueReceive(qh, &event, portMAX_DELAY);
    assert(rc == pdPASS); 
    if (event){ 
      /*
      This means that if we did not touch the button, the LED would turn on during the first iteration; if we continued not to press it, thanks to line 13, no event would occur, and the LED would therefore remain on. Conversely, if we were to press the button, event = 0 because the button uses a pull-up resistor, so the if condition would not be met; depending on the situation, the LED would remain either on or off, and that is not what we want.
      My idea: if we used `if (!event)` instead of `if (event)`, then it would make sense: if the button is not pressed, the LED's state remains static, but when it is pressed, the LED's state toggles.
      */
      led ^= true; 
      digitalWrite(GPIO_LED, led);
    }
  }
}

void setup() {
  int app_cpu = xPortGetCoreID();
  BaseType_t rc;
  TaskHandle_t taskh;

  delay(2000);
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
    &taskh,
    /*
    Looking also at line 72: we are writing the handles of two different tasks to the same memory address.
    The led_task handle overwrites the debounce_task handle, which would cause us to lose control of the debounce_task.
    If we do not need to control either of them, we can simply use `nullptr`; otherwise, we need to create two separate handles.
    */
    app_cpu
  );
  assert(rc==pdPASS);
  assert(taskh);

    rc = xTaskCreatePinnedToCore(
    led_task,
    "led",
    2048,
    nullptr,
    1,
    &taskh,
    app_cpu
  );
  assert(rc==pdPASS);
  assert(taskh);
}

void loop(){
  vTaskDelete(nullptr); 
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