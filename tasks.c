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
        loopTask, // The name of the function the task needs to execute.
        "loopTask", // The assigned name of the task.
        8192, // Task size (bytes).
        NULL, // No parameters needed in the function.
        1, // Priority.
        &loopTaskHandle, // loopTaskHandle's address, where we are going to save loopTask's reference.
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
  TaskHandle_t taskh; // Task handle. 'TaskHandle_t' is an RTOS data type which contains every task internal structure's address.
};

// Creating an s_led array.
static s_led leds[3] = {{LED1, 0, 500, 0}, {LED2, 0, 200, 0}, {LED3, 0, 750, 0}}; // 'static' is a keyword to state that the data can only be used in this .cpp file.

// Defining the task's function.
static void led_task_func(void *argp){ // A single address for the s_led instance is sufficient to access all its variables (because they are stored in adjacent cells in sRAM).
  s_led *ledp = (s_led*)argp; // Type casting: the value inserted as a function paramter turns into an s_led pointer.
  unsigned stack_hwm = 0, temp; // Declaring two unsigned integers at the same time (stack_hwm, temp).

  delay(1000);

  for(;;){
    digitalWrite(ledp->gpio, ledp->state ^= 1); // digitalWrite(ledGPIO, HIGH/LOW) but instead of 'HIGH/LOW', '^=' (XOR) is used (if the LED is off, it will be turned on and viceversa). '->' is only used with pointers to struct.
    temp = uxTaskGetStackHighWaterMark(nullptr); // FreeRTOS function that, thanks to 'nullptr' or 'NULL' as parameter, returns the minimum stack memory level left for this stack since the task has started.
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
  printf("app_cpu is %d (%s core)\n", app_cpu, app_cpu > 0? "Dual" : "Single"); // 'app_cpu > 0? "Dual" : "Single"' equals to 'if(app_cpu>0){printf("Dual")}else{printf("Single")}'.

  printf("LEDs on gpios: ");
  for (auto& led : leds){ 
    /*
    `for(auto& led : leds)` is quite complex. Three actions take place: first, `auto` resolves to `s_led` (effectively becoming `s_led& led : leds`), matching the data type of `leds`; next, each `leds[i]` in `leds` is aliased as `led` (thanks to `s_led& led`); finally, the loop iterates for each leds[i]. Had there been just `auto` instead of `auto&`, a copy of each `leds[i]` named `led` would have been created (pass-by-value), meaning the following code would have operated on copies rather than the original elements.
    */
    pinMode(led.gpio, OUTPUT);
    digitalWrite(led.gpio, LOW);
    xTaskCreatePinnedToCore(
      led_task_func,
      "led_task", // There will be three tasks with the same name, but this is not a problem in FreeRTOS.
      2048,
      &led, // Passing the memory address of the LED that needs to blink to each task.
      1,
      &led.taskh,
      app_cpu
    );
    printf("%d ", led.gpio);
  }
  putchar("\n"); // This way, any further messages will be printed on a clean line.
  }
  
}

void loop() {
  delay(1000);
}
