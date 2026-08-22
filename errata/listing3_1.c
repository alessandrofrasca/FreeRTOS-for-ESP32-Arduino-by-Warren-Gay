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