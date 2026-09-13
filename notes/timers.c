/*
Regarding time management in the Arduino-ESP32 environment, there are three different approaches:
- Arduino API (for instance, delay());
- ESP32 hardware functions;
- FreeRTOS API functions.
Here is a brief example of how ESP32 delay() function is defined:
*/
void delay(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS); // vTaskDelay() is a FreeRTOS function that delays the task (by blocking it) for a specified number of ticks. The number of ticks is calculated by dividing the number of milliseconds by the number of milliseconds per tick (portTICK_PERIOD_MS, which is around 1000 ms on ESP32).
}
// FreeRTOS allows us to use a timer callback, that is a function that executes as soon as the timer expires (for example: as soon as the morning alarm goes off, it says "hello"), without blocking the task and the CPU. A timer callback might look like this:
void my_timer_cb(TimerHandle_t xTimer) {
    // This function will be called when the timer expires.
    printf("Timer expired!\n");
}
// Important note: the timer callback function is executed and managed by the Timer Service Task (or "Tmr Svc" task), a FreeRTOS background task. It has an initial free stack of approximately 1480 bytes, so we got to be careful with memory management. Furthermore, it is important to ensure that the timer callback function executes quickly and does not cause the Tmr Srv task to block (we should not use delay() or vTaskDelay()): if the task were to block (or the timer callback took a long time), other timers might expire in the meantime!

/*
An important concept is the "timer ID", which is a pointer to void. We assign it to a timer during its creation; if it is not needed, it can simply be set to `nullptr`.
It is useful if we want to use a single timer callback function for multiple timers (for instance, if we wanted to turn on four different LEDs when four different timers expire, it is efficient to use one callback function that can determine which LED to act upon based on the each timer's handle).
*/
void my_timer_cb(TimerHandle_t xTimer) {
    void* timer_id = pvTimerGetTimerID(xTimer); // This function returns the timer ID (a pointer to void) of the timer that expired. We can use it to determine which timer expired and take appropriate action. We may use casting if we want to.
    if (timer_id == nullptr) {
        printf("Timer expired with no ID!\n");
    } else {
        printf("Timer expired with ID: %p\n", timer_id); // %p is the format specifer for pointers, while %u is the format for unsigned integers.
    }
}

/*
Within FreeRTOS, there are two types of timers:
- one-shot timers: they do not automatically restart upon expiration;
- auto-reload timers: they automatically restart upon expiration.
A timer's state can be:
- dormant: the timer is not active (time is not elapsing);
- running: the timer is active/running.
As soon as a timer is created using xTimerCreate(), it is in the dormant state. Upon calling xTimerStart(), xTimerReset(), or xTimerChangePeriod(), it enters the running state: if it is a one-shot timer, it returns to the dormant state when it expires; if it is an auto-reload timer, it remains running. The xTimerStop() function can be used to stop the timer, returning it to the dormant state.
*/

// In order to create a static timer:
StaticTimer_t tobj; // (Just as there is `StaticQueue_t qobj` for a static queue) this is the timer's control block, which will be used to store the timer's state and other information. It must be declared as a global variable or static variable (otherwise it would be destroyed when the function that created it returns).
TimerHandle_t th; // This is the timer's handle, which will be used to refer to the timer in other functions. It must be declared as a global variable or static variable (otherwise it would be destroyed when the function that created it returns).
struct s_user_data{
    members...
} socket1; // This is a structure that will be used to store user data that will be passed to the timer callback function. It must be declared as a global variable or static variable (otherwise it would be destroyed when the function that created it returns). 'socket1' is an instance of the structure 's_user_data'. If I had used `typedef`, I would have defined a new variable type corresponding to the struct; however, since I did not use `typedef`, `socket1` is simply an instance of `s_user_data` (like `int x`).

th = xTimerCreateStatic( // The timer gets created in the dormant state.
    "my_timer1", // Timer name (for debugging purposes).
    timer_period_ticks, // Timer period in ticks. If we want to express it in milliseconds, the pdMS_TO_TICK(ms) macro can be used (even though if we write in a value less than 1, it will be rounded to 0 ticks, which is not acceptable).
    pdFALSE, // Auto-reload (pdTRUE) or one-shot (pdFALSE).
    &socket1, // Timer ID (pointer to void) that will be passed to the timer callback function. 'socket1' is not the timer ID itself, its address is the timer ID.  If not used, 'nullptr' will do.
    my_timer_cb, // Timer callback function that will be called when the timer expires.
    &tobj // Pointer to the timer's control block (StaticTimer_t) that will be used to store the timer's state and other information.
);
assert(th); // Check whether th != nullptr, which means that the timer was created successfully. If th == nullptr, the timer was not created successfully (for example, if there was not enough heap memory available to create the timer). In this case, we should handle the error appropriately (for example, by printing an error message and/or returning from the function).

// Creating a dynamic timer is just as easy; the only difference is that the timer object is not needed:
TimerHandle_t th;
struct s_user_data{
    members...
} socket1;

th = xTimerCreate(
    "my_timer1",
    timer_period_ticks,
    pdFALSE,
    &socket1,
    my_timer_cb
);
assert(th);

// To start a timer, xTimerStart() and xTimerReset() are equivalent. In both cases, if the timer is already running, it will be completely restarted; if dormant, it will be started:
BaseType_t xTimerStart(
    TimerHandle_t xTimer,
    TickType_t xTicksToWait // Both functions send messages (via a queue that has a default depth of 10 items) to the Tmr Svc task; if the queue is full and the specified number of ticks to wait has elapsed, the functions fail.
);

BaseType_t xTimerReset(
    TimerHandle_t xTimer,
    TickType_t xTicksToWait
);
// xTimerChangePeriod() works like the other two: if the timer is already running, it restarts it; if it is dormant, it activates it. The only difference is that the timer's duration (in ticks) is changed.
BaseType_t xTimerChangePeriod(
    TimerHandle_t xTimer,
    TickType_t xNewPeriod, // New timer period in ticks.
    TickType_t xTicksToWait
);

/*
Before moving on to the timer demonstration, it is worth knowing that C++ introduces classes, unlike C. A class is essentially like a struct; the only difference is that, in addition to variables, it can contain functions known as "methods." Any specific instance of a class is called an object.
Initially, the `alert1` object is created with `count = 0` and `state = true`, so the LED starts in the on state. The timer expires every 50 ms, toggling the LED's state. Once `count` reaches 9 (therefore, after five full LED blinks), the LED remains off for 550 ms. When `count` reaches 10, it turns on and resumes flashing every 50 ms.
Meanwhile, the loopTask waits for 50 seconds, then stops the timer and turns off the LED for 20 seconds. At the 70th second (loopcount = 70 resets to loopcount = 0), the LED is turned backon and the timer and the object (just in case count != 0)are restarted; it will flash at 50 ms intervals, beginning the loop again.
*/

#define GPIO_LED 12
class AlertLED { // In this demonstration, the class is used to access the LED through the timer.
  TimerHandle_t thandle = nullptr; // We do not need a timer's handle.
  volatile bool state;
  /*
  Sometimes, the compiler copies variables from RAM into CPU registers, aiming to optimize and speed up program execution (for example, given `while(state == false){}`, the compiler, seeing no code lines within the while loop, might copy the value of `state` into a CPU register and read only that value; consequently, if the value of `state` were to change in RAM, the compiler would continue reading the old value stored in the CPU register).
  The `volatile` keyword prevents this optimization by telling the compiler that the variable may be subject to change.
  */
  volatile unsigned count;
  unsigned period_ms;
  int gpio;

  // The methods are defined later on.
  void reset(bool s); // Internal method to the class, it will be used to reset the state of the objects.
  // Everything before 'public' is considered 'private': variables and methods visible only within the class. No other part of the program can modify them directly (for example, we cannot do `myLED.state = true` or 'myLED.reset(true)' from outside). The `reset()` method is private because it is intended as an internal helper. Private methods can sometimes be triggered by using public methods.
  public:
    AlertLED(int gpio, unsigned period_ms=1000); // This is the class constructor. It is used to create instances of the class, the so called 'objects' (for example, AlertLED redLED(12, 500)). If we do not specify `period_ms`, the default value (in this case, 1000) will be used.
    void alert(); // This method will be called when we want to activate the alert mode.
    void cancel(); // This method will be called when we want to disable the alert mode.

    static void callback(TimerHandle_t th); // Static method. This means that there is no attached object when it is called, since a static method does not belong to the individual object, but to the class.
    /*
    When we call led1.alert(), the compiler actually translates that call into: AlertLED_alert(&led1);
    In other words, it passes a hidden parameter (the 'this' pointer) that tells the function: "operate on led1's variables." A static method does not receive the hidden 'this' parameter. Effectively, it is a function identical to a global C function, simply encapsulated within the AlertLED class block to keep the code organized (for example, we might type AlertLED::callback() to call the static method, since it does not require an object; it just looks like a global function).
    'static' has several meanings. For a global function, it means it can only be used within the file where it is defined; for a variable, it means it is stored in static SRAM; for a class method, it means it is a static method.
    */
};

// Defining AlertLED's constructor:
AlertLED::AlertLED(int gpio, unsigned period_ms){
  this->gpio = gpio;
  this->period_ms = period_ms;
  pinMode(this->gpio, OUTPUT);
  digitalWrite(this->gpio, LOW);
}

// Defining internal method to reset values:
void AlertLED::reset(bool s){
  state = s; // 'this->state' would be equivalent: we can avoid using 'this' when the function parameter names and the class member names are not the same.
  count = 0;
  digitalWrite(this->gpio, s? HIGH:LOW); // We could have avoided 'this'. If (state == true){HIGH}.
}

// Defining the method to start the alert:
void AlertLED::alert(){
  if (!thandle){ // This is our case.
    thandle = xTimerCreate(
      "alert_tmr",
      pdMS_TO_TICKS(period_ms / 20),
      pdTRUE, // Since we are dealing with an auto-reload timer.
      this, // Using the object's address as timer ID.
      AlertLED::callback
    );
    assert(thandle);
  }
  reset(true); // We could have used 'AlertLED::reset(true)' but, since we are already within AlertLED, there is no need. Since we typed 'reset(true)', count = 0 and the LED will be turned on.
  xTimerStart(thandle, portMAX_DELAY);
}

// Method to stop an alert:
void AlertLED::cancel(){
  if (thandle){ // This if statement is not necessary, but it is a good practice in order to increase code safety.
    xTimerStop(thandle, portMAX_DELAY);
    digitalWrite(gpio, LOW); // Turning the LED off after stopping the timer.
  }
}

// Defining static method (therefore, we cannot use 'this' normally, we must use the timer ID), acting as the timer callback function:
void AlertLED::callback(TimerHandle_t th){
  AlertLED* obj = (AlertLED*)pvTimerGetTimerID(th); // Timer ID.

  assert(obj->thandle == th);
  obj->state ^= true; // During its first execution, the state gets turned into 'false'.
  digitalWrite(obj->gpio, obj->state? HIGH:LOW); // If (state==true){HIGH} else LOW.

  if (++obj->count >= 5*2){ // If the LED flashes 5 or more times. '++obj->count' means that, just once (that is, only once per function execution), we treat obj->count as obj->count + 1.
    obj->reset(true);
    xTimerChangePeriod(th, pdMS_TO_TICKS(obj->period_ms / 20), portMAX_DELAY);
  } else if (obj->count == 5 * 2 - 1){
    xTimerChangePeriod(th, pdMS_TO_TICKS(obj->period_ms / 20 + obj->period_ms / 2), portMAX_DELAY);
    assert(!obj->state); // If state != 0 then abort().
  }
}

// Global objects:
static AlertLED alert1(GPIO_LED, 1000);
static unsigned loop_count = 0;

void setup(){
  // delay(2000); // Allowing USB to connect.
  alert1.alert();
}

void loop(){
  if (loop_count >= 70){
    alert1.alert();
    loop_count = 0;
  }
  // The LED continues to flash for 50 seconds. At the 50th second, the timer stops and the LED turns off. The LED remains off for 20 seconds; at the 70th second, the alarm restarts from zero.
  delay(1000);

  if (++loop_count >= 50){
    alert1.cancel();
  }
}

/*
As we have already seen, delay() contains vTaskDelay().
Other functions that cause a task to block are:
*/

TickType_t xTaskGetTickCount(); // It returns the number of ticks since the FreeRTOS scheduler started (i.e., even before `setup()` executes). 
/*
We should watch out out for overflows (if we have 4 bits and reach the maximum bit representation `1111`, adding another bit results in `0000`, causing an overflow). One way to handle overflows:
*/
TickType_t tick1, tick2, delta;
tick1 = xTaskGetTickCount();
tick2 = xTaskGetTickCount();
if (tick2 >= tick1){ // If there is no overflow, tick2 is going to be bigger than tick1, at most.
  delta = tick2 - tick1;
} else delta = tick2 + 1 + (~TickType_t(0) - tick1); // This is a case of an overflow. '+1' is essential because tick2 represents (n-1) ticks after the overflow.

// vTaskDelayUntil() allows us to block a task until a very specific tick time: the first parameter points to the system tick count (updated each time the function is called) from the previous time the function was called, while the second parameter defines the tick interval (which is also the amount by which the first parameter will increase). 'First parameter + second parameter = the tick time at which the task will be unblocked'.

vTaskDelayUntil(TickType_t *pxPreviousWakeTime, TickType_t xTimeIncrement); 

// The following demonstration highlights the difference between delay() and vTaskDelayUntil(): delay() can indirectly result in a longer physical execution time for a function, whereas vTaskDelayUntil() does not.

#define GPIO_LED1 12
#define GPIO_LED2 13

static volatile bool startf = false;
static TickType_t period = 250;

static void big_think(){ // This is an arbitrary function, but one that takes a long time to execute.
  for (int x = 0; x<40000; x++){
    __asm__ __volatile__ ('nop');
  }
}

static void led1(void* argp){
  bool state = true;

  while (!startf) // This empty while loop is important: if startf remains false (which it will until we are ready to make both LEDs blink simultaneously), the task remains stuck in the while loop. This ensures that the LEDs start blinking at the exact same moment (since FreeRTOS might let a task begin before the other).
    ;

  for (;;){
    state ^= true;
    digitalWrite(GPIO_LED1, state);
    big_think();
    delay(period); // Normally, we would expect the LED to change state every 250 ticks. However, the big_think() function takes a significant amount of time, thus slowing down the state change.
  }
}

static void led2(void* argp){
  bool state = true;

  while (!startf)
    ;
  
  TickType_t ticktime = xTaskGetTickCount();
  for (;;){
    state ^= true;
    digitalWrite(GPIO_LED2, state);
    big_think();
    vTaskDelayUntil(&ticktime, period); // Regardless of how long the big_think() function takes to execute (except if it is over 250 ticks), the LED will change state exactly after 250 ticks.
  }
}

void setup(){
  int app_cpu = xPortGetCoreID();
  BaseType_t rc;

  delay(2000); // Allowing USB to connect.

  pinMode(GPIO_LED1, OUTPUT);
  digitalWrite(GPIO_LED1, HIGH);
  pinMode(GPIO_LED2, OUTPUT);
  digitalWrite(GPIO_LED2, HIGH);

  rc = xTaskCreatePinnedToCore(
    led1,
    "led1",
    1024,
    nullptr,
    1,
    nullptr,
    app_cpu
  );
  assert(rc == pdTRUE);

  startf = true;
}

void loop(){
  delay(50); // However, `vTaskDelete(nullptr)` or any other function that limits the `loopTask` time within the scheduler is also fine.
}