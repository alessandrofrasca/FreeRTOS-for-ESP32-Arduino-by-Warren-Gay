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
// Important note: the timer callback function is executed and managed by the Timer Service Task (or "Tmr Svc" task), a FreeRTOS background task. It has an initial free stack of approximately 1400 bytes, so we got to be careful with memory management. Furthermore, it is important to ensure that the timer callback function executes quickly and does not cause the Tmr Srv task to block (we should not use delay() or vTaskDelay()): if the task were to block (or the timer callback took a long time), other timers might expire in the meantime!

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

// Creating a static timer is just as easy; the only difference is that the timer object is not needed:
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
The following demonstration features an LED that turns on and off every 1000 ms by default. If an error occurs, it will flash rapidly five times, then turn off and repeat the sequence.
*/