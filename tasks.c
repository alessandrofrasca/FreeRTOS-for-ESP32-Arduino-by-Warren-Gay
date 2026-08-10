/*
A process is an application running in isolation, as it possesses its own logical memory. 
Somebody can choose to create subprocesses known as threads within a process. These are significant because creating or destroying a process is a resource-intensive operation for the CPU and RAM (due to the need to allocate logical memory), whereas creating or stopping a thread is far simpler. 
Processes cannot exchange data because they run in isolated environments, whereas threads can, as they share access to the logical memory of their parent process.
When the process is handed over to the scheduler, threads are executed individually (which helps prevent infinite loops), and multiple threads can run simultaneously on multicore CPUs. Notably, the scheduler's queue consists of the collective set of threads originating from various processes.
In FreeRTOS, it is as if there were a single, large process containing the threads (tasks). Consequently, all tasks have access to the same logical memory.
Running my program requires a task, and there are background FreeRTOS tasks; some of them handle WiFi, timers, and Bluetooth. loopTask is the main Arduino task that calls setup() and loop().
The stack is a region of logical memory reserved for a single thread (in our case, a task).
The stack persists while the task is running; when the task completes its work, the associated stack is also removed.
When a function is executed within a task, a stack frame is created on the stack; this frame exists as long as the function is running.
Tasks also have a priority level (with 0 being the lowest), and their identification numbers are assigned in ascending order based on the time of creation. Another key detail regarding the dual-core ESP-32 (CPU 0 and CPU 1) is that each task can run on either CPU 0 (which by default handles Espressif-specific tasks related to communication, such as WiFi and Bluetooth) or CPU 1 (which by default executes application/software tasks).
Here is an example of creating a task in C++:
*/
void loopTask(void *pvParameters){ // Defining my task's function. Using a `void` pointer (which means the function can accept any pointer type) is provided for by the task syntax. It does not matter whether my task does not need parameters.
    
    setup(); // It is executed only once, each time the ESP-32 board is reset.
    for (;;){
        loop(); // Since the condition field is NULL, the condition is always true (equivalent to while(true) or while(1)), so the loop() function will execute indefinitely.
    }
}

extern "C" void app_main(){
    /*
    Since we are writing in C++, two functions can share the same name provided they require different parameters, which is not allowed in C (the language used by ESP-32 and FreeRTOS) because the C++ compiler performs "name mangling" (altering function names during compilation). To prevent name mangling, we instruct the compiler to use C language compilation rules.
    void app_main() is the equivalent of int main() for operating systems running on the ESP-32. When the ESP-32 board starts up, the operating system automatically creates main, which is a task that executes app_main(). Once app_main() finishes executing, the main task is removed.
    */
    initArduino(); // Arduino's initialization (therefore, hardware's initialization).
    xTaskCreatePinnedToCore( // Calling a function used to create tasks (on non-Espressif platforms it might just be "xTaskCreate"), whose parameters are:
        loopTask, // The name of the function the task needs to execute. In FreeRTOS, each task has got a single function.
        "loopTask", // The assigned name of the task.
        8192, // Task size (bytes).
        NULL, // No parameters needed in the function, but in general, we need to enter the parameters' addresses that serve as input for the task function.
        1, // Priority.
        &loopTaskHandle, // loopTaskHandle's address, where we are going to save loopTask's reference. Handles are useful to manage tasks (letting us stop and resume them, delete them, etc.).
        1 // Arduino core containing the running task.
    );
}
// Task demonstration:
// Defining GPIOS.
#define LED1 12
#define LED2 13
#define LED3 15

// Struct definition is easier in C++.
struct s_led{
  byte GPIO; // LED GPIO number. 'byte' does not exist in C++, but in this environment it is an alias for 'uint8t' (typedef uint8t byte), which stands for 'unsigned 8 bit integer'.
  byte state; // LED state.
  unsigned napms; // Delay to use (ms). In C and C++, 'unsigned' without anything else stands for 'unsigned int'.
  TaskHandle_t taskh; // Task handle. 'TaskHandle_t' is an RTOS data type which contains the task internal structure's address.
};

// Creating an s_led array.
static s_led leds[3] = {{LED1, 0, 500, 0}, {LED2, 0, 200, 0}, {LED3, 0, 750, 0}}; // 'static' is a keyword to state that the data can only be used in this .cpp file.

// Defining the task's function.
static void led_task_func(void *argp){ // A single address for the s_led instance is sufficient to access all its variables (because they are stored in adjacent cells in sRAM).
  s_led *ledp = (s_led*)argp; // Type casting: the value inserted as a function parameter turns into an s_led pointer.
  unsigned stack_hwm = 0, temp; // Declaring two unsigned integers at the same time (stack_hwm, temp).

  delay(1000); // A delay here is necessary to prevent the outputs of the task (started via xTaskCreatePinnedToCore()) and those of setup() from getting mixed up. With delay(1000), we slow down the task.

  for(;;){
    digitalWrite(ledp->gpio, ledp->state ^= 1); // digitalWrite(ledGPIO, HIGH/LOW) but instead of 'HIGH/LOW', '^=' (XOR) is used (if the LED is off, it will be turned on and viceversa). '->' is only used with pointers to struct.
    temp = uxTaskGetStackHighWaterMark(nullptr); // FreeRTOS function that, thanks to 'nullptr' or 'NULL' as parameter (aiming to the current task), returns the unused stack memory level left for this stack since the task has started (know as stack hwm).
    if (!stack_hwm || temp < stack_hwm){ // '!stack__hwm' equals to 'stack_hwm==0', therefore it is true through the first iteration.
      stack_hwm = temp;
      printf("Task for gpio %d has stack hwm %u\n", ledp->gpio, stack_hwm); // '%u' references to unsigned data type.
    }
    delay(ledp->napms);
  }
}

void setup() {
  int app_cpu = 0; // CPU number.

  delay(500); // Pause for serial setup. When I connect the ESP-32 to the computer, it takes a fraction of a second to synchronize the connection.

  app_cpu = xPortGetCoreID(); // FreeRTOS function that returns CPU's ID (0 for CPU 0, 1 for CPU 1) to let me know on which CPU loopTask is running.
  printf("app_cpu is %d (%s core)\n", app_cpu, app_cpu > 0? "Dual" : "Single"); // 'app_cpu > 0? "Dual" : "Single"' equals to 'if(app_cpu>0){printf("Dual")}else{printf("Single")}'. We could have just chosen 'tskNO_AFFINITY' to assign the task to an available CPU.

  printf("LEDs on gpios: ");
  for (auto& led : leds){ 
    /*
    `for(auto& led : leds)` is quite complex. Three actions take place: first, `auto` resolves to `s_led` (effectively becoming `s_led& led : leds`), matching the data type of `leds`; next, each `leds[i]` in `leds` is aliased as `led` (thanks to `s_led& led`); finally, the loop iterates for each leds[i] (known as 'led'). Had there been just `auto` instead of `auto&`, a copy of each `leds[i]` named `led` would have been created (pass-by-value), meaning the following code would have operated on copies rather than the original elements.
    */
    pinMode(led.gpio, OUTPUT);
    digitalWrite(led.gpio, LOW);
    xTaskCreatePinnedToCore(
      led_task_func,
      "led_task", // There will be three tasks with the same name, but this is not a problem in FreeRTOS.
      2048,
      &led, // Passing the memory address of the LED that needs to blink to each task.
      1,
      &led.taskh, // Task handle's memory address.
      app_cpu
    );
    printf("%d ", led.gpio);
  }
  putchar('\n'); // This way, any further messages will be printed on a clean line. Notice how we use '' instead of "".
}


void loop() {
  delay(1000); // loopTask is "suspended" for a full second each time it executes, so as not to slow down the LED tasks.
}

/*
In the ESP32, SRAM is divided into two parts: static (for storing global variables and static data) and dynamic (also known as the heap).
The heap is, in turn, physically divided into several areas based on the capabilities of those memory zones.
The heap contains DRAM (Data RAM) for storing data, IRAM (Instruction RAM) for storing only executable code, and D/IRAM—RAM that can be used for both instructions and data; finally, external SPI RAM can also be used with the ESP32.
void* malloc(size_t size) is useful to allocate a contiguous block of heap memory. The number of contiguous bytes required is passed as a parameter, and a void pointer to the memory address of the block's first byte is returned (or `NULL` if available heap memory has been exhausted). 'size_t' represents the number of bytes as an unsigned integer.
void free(void* ptr) is useful to free a block of memory that we had reserved. It accepts the memory address of the block's first byte as a parameter (it accepts any type of pointer as a parameter.)
void* heap_caps_malloc(size_t size, uint32_t caps) is useful to allocate a specific zone of heap memory. `uint32_t` guarantees a 32-bit unsigned integer, whereas `unsigned` guarantees an integer with a variable size that depends on the CPU used for compilation.
Same goes for void heap_caps_free(void* ptr).
In C++, the `static` keyword serves two purposes: it restricts the scope of the defined data to the current `.cpp` file, and it instructs the compiler to avoid placing the data on the temporary stack or allocating it from the heap. Instead, the data is assigned a fixed memory address at compile time, and it remains there as long as the device is powered on.
When creating a task, a stack and a TCB (Task Control Block, which stores the task's priority, name, state, and other information) are required. If we choose to allocate static memory for the task's stack and TCB, the memory needed for the task's execution remains available regardless of what happens to the heap (such as corruption or memory exhaustion).
Unlike the heap, the static SRAM section allocates memory to the task immediately during code compilation, rather than taking time to search for contiguous free memory blocks; this leads to another advantage: if there is insufficient space in the static SRAM, the code fails to compile (since the memory block search occurs at compile time) whereas with the heap, allocation happens after compilation, while the program is already running. Another advantage is the absence of fragmentation in static SRAM (given that everything is stored in consecutive, contiguous blocks), a problem that does occur with the heap.
The functions that allow us to allocate static memory to a task are xTaskCreateStaticPinnedToCore() (Espressif-only) and xTaskCreateStatic(). Unlike xTaskCreatePinnedToCore() or xTaskCreate(), the return value is the task handle.
*/
// Static task demonstration:
// It is important to use 'static' even before inserting stack[0] as a xTaskCreateStatic()'s parameter. In this example, we want to obtain a 2048-byte static stack and to do so, we simply divide the required number of bytes by the size of a single `StackType_t` element to determine how many `StackType_t` elements to allocate to the stack (i.e., the array size). We cannot simply declare `static StackType_t stack[2048]`, because that would create 2,048 elements, each occupying 4 bytes of memory in the case of the ESP32.
static StackType_t stack[2048/sizeof(StackType_t)];
static StackType_t tcb;
TaskHandle_t taskh;

taskh = xTaskCreateStatic(
  task_func,
  "statictsk",
  2048, // Stack size.
  &args,
  1, // Priority.
  &stack[0], // Or just 'stack' will do, since an array's name already points to its first element.
  &tcb  
);
/*
We can delete tasks. Usually, they delete themselves when they finish their work.
A task can delete itself or be deleted by another task.
At line 104, loop() is a problem, as it wastes stack space and time.
*/
// Self-deleting a task demonstration:
static void led_task_func(void *argp){ 
  s_led *ledp = (s_led*)argp;
  unsigned stack_hwm = 0, temp;

  delay(1000); 

  for(;;){
    digitalWrite(ledp->gpio, ledp->state ^= 1); 
    temp = uxTaskGetStackHighWaterMark(nullptr); 
    if (!stack_hwm || temp < stack_hwm){ 
      stack_hwm = temp;
      printf("Task for gpio %d has stack hwm %u. Remaining heap: %u\n", ledp->gpio, stack_hwm, unsigned(xPortGetFreeHeapSize())); // <-- In C++, we can cast using `unsigned(expression)`, not necessarily `(unsigned)expression`.
    }
    delay(ledp->napms);
  }
}
void setup() {
  int app_cpu = 0; 

  delay(500); 

  app_cpu = xPortGetCoreID(); 
  printf("app_cpu is %d (%s core)\n", app_cpu, app_cpu > 0? "Dual" : "Single");

  printf("LEDs on gpios: ");
  for (auto& led : leds){ 
    pinMode(led.gpio, OUTPUT);
    digitalWrite(led.gpio, LOW);
    xTaskCreatePinnedToCore(
      led_task_func,
      "led_task", 
      2048,
      &led, 
      1,
      &led.taskh, 
      app_cpu
    );
    printf("%d ", led.gpio);
  }
  putchar('\n');
  printf("There are %u heap bytes available.\n", unsigned(xPortGetFreeHeapSize())); // <--
}

void loop(){
  // The task deletes itself (in this case, loopTask deletes itself) as soon as vTaskDelete() is called. However when the stack memory gets released depends upon who the caller is.
  //If, on the other hand, the task is deleted by another task, the task's stack memory is released immediately.
  // We can also delete a static task using the vTaskDelete() function: the stack memory will not be released (once it is occupied, we cannot free it) until the ESP32 is off, but the TCB will be modified to make it impossible to associate the deleted task with its memory again.
  vTaskDelete(nullptr); // If we wanted to delete another task, instead of `nullptr`, I would need to insert the name of the handle for the task to be deleted.
}
// It is also possible to suspend and resume a task: during suspension, the stack memory is not freed because the task still exists.
// Task suspension demonstration (referring to the lines of code above):
 
void loop(){
  printf("Suspending LED2's task.\n");
  vTaskSuspend(leds[1].taskh);
  delay(5000);
  printf("Resuming LED2's task.\n");
  vTaskResume(leds[1].taskh);
}
// If it is not possible to access a task's handle within the task function, the following function can be used:
TaskType_t taskh = xTaskGetCurrentTaskHandle();

/*
The scheduler assigns a time slice to each running task. But how long is the time slice?
To measure it, an oscilloscope and two competing tasks are needed: one writes HIGH to a GPIO pin, and the other (running on the same CPU) writes LOW to the same pin.
*/