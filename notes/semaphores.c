/*
A semaphore is a data structure that can assume two possible states:
- 'empty', also known as 'not given';
- 'full', also known as 'given'.
Semaphores might be:
- binary semaphores: this is like a queue with a depth of 1 item, where the specific item itself does not matter (this is why the item size is 0 bytes). Once created, the semaphore is in the 'empty' state (for example: if task A creates the semaphore, initially empty, and Task B attempts to "take" it, the attempt will fail and the task will be blocked or an error will occur, depending on specific parameters; if task A fills ("gives") the semaphore and task B attempts to acquire ("take") it, task B will not be blocked, and the semaphore will be automatically emptied). If we were to attempt to fill ('give') a semaphore that is already 'full' ('given), we would receive the pdFALSE error. A binary semaphore can only block a single task at a time.
- counting semaphores: these are like queues with a depth greater than one item. They can block multiple tasks simultaneously. The counting of "given items" to the semaphore can be done either by "counting up from zero" (where the semaphore starts empty; each "give" increments the count, and each "take" decrements it) or by "counting down from an initial quantity" (where the semaphore starts with a specific number of "given items"; each "give" increases the count, and each "take" decreases it).
When using counting semaphores, we must be aware of race conditions (where two tasks attempt to access the same resource or item). Consider this scenario: only one item remains in the semaphore. Before taking it, task A checks (using an if statement) that `uxSemaphoreGetCount() > 0` (a function returning the number of items currently in the semaphore). However, before task A can proceed to take the item, the scheduler might switch execution to task B, which then takes the item instead.
*/

/*
Demonstration of a binary semaphore.
For this demonstration, the HC-SR04 module is used (it emits ultrasound, which is sound with a frequency higher than 20,000 Hz). Initially, the ESP32 requires a ping of the environment by setting the module's 'trigger' pin to 'HIGH' for a short duration. Then, the HC-SR04 holds the 'echo' pin 'HIGH' for the duration of the sound emission, until the echo is received. Once the time interval is recorded, the 'echo' pin goes 'LOW', and the distance to the obstacle is calculated based on the speed of sound (343 m/s). The SSD1306 OLED screen can be used to view the results; otherwise, the Arduino IDE Serial Monitor works fine. The HC-SR04 works at 5V, so we need to connect the ESP32's 5V pin (which directly routes the 5V power from the computer's USB connection to the HC-SR04 without damaging the ESP32) to the HC-SR04's 5V pin.
*/

#define GPIO_LED 12
#define GPIO_TRIGGER 25
#define GPIO_ECHO 26

typedef unsigned long usec_t;
static SemaphoreHandle_t barrier; // Semaphore's handle.
static TickType_t repeat_ticks = 1000;

static void report_cm(usec_t usecs){
  unsigned cm, tenths;
  cm = usecs * 10ul / 58ul; // 'usecs' represents the microseconds measured by the ESP32, and 'usecs / 58' would yield the exact distance in cm; however, in C++, the value would be rounded. By calculating 'usecs * 10 / 58', we obtain a more precise approximation. The 'ul' suffix attached after each number tells the compiler to define that number as an unsigned long, avoiding the need for casting (which would take longer).
  tenths = cm % 10; // 'cm' represents 10 times the actual centimeters. Example: 'cm = 34' means we have 3.4 cm. To isolate 0.4, we can use 34 % 10.
  cm /= 10; // The millimeters will be truncated here.

  printf("Distance %u.%u cm, %u usecs\n", cm, tenths, usecs);
}

static void range_task(void* argp){
  BaseType_t rc;
  usec_t usecs;

  for (;;){
    rc = xSemaphoreTake(barrier, portMAX_DELAY); // Waiting for the semaphore to be full.
    assert(rc == pdTRUE);
    digitalWrite(GPIO_LED, HIGH);
    digitalWrite(GPIO_TRIGGER, HIGH);
    delayMicroseconds(10); // 10 microseconds are enough for the HC-SR04 to process the ESP32's request.
    digitalWrite(GPIO_TRIGGER, LOW);

    // Listening for echo:
    usecs = pulseInLong(GPIO_ECHO, HIGH, 50000); 
    // Unlike pulseIn(), pulseInLong() is designed for longer-duration pulses. The first parameter specifies the pin on which to measure the time; the second parameter indicates the state to measure (in this case, 'HIGH' means measuring the microseconds during which GPIO_ECHO receives a signal; the measurement ends as soon as GPIO_ECHO detects 'LOW'); and the third parameter specifies the maximum duration in microseconds (if this limit is exceeded, for instance, when the obstacle is too far away or absent, the function returns 0).
    digitalWrite(GPIO_LED, LOW);

    if (usecs > 0 && usecs < 50000UL){
      report_cm(usecs);
    } else {
      printf("Echo not detected.\n");
    }
  }
}

static void sync_task(void* argp){
  BaseType_t rc;
  TickType_t ticks;

  delay(1000);

  ticks = xTaskGetTickCount();
  for (;;){
    vTaskDelayUntil(&ticks, repeat_ticks); // The semaphore fills up every second.
    rc = xSemaphoreGive(barrier);
    assert(rc == pdTRUE);
  }
}

void setup(){
  int app_cpu = xPortGetCoreID();
  BaseType_t rc;
  TaskHandle_t range_taskh;
  TaskHandle_t sync_taskh;

  barrier = xSemaphoreCreateBinary();
  

  pinMode(GPIO_LED, OUTPUT);
  digitalWrite(GPIO_LED, LOW);
  pinMode(GPIO_TRIGGER, OUTPUT);
  digitalWrite(GPIO_TRIGGER, LOW);
  pinMode(GPIO_ECHO, INPUT_PULLUP);

  delay(2000); // Allowing USB to connect.

  rc = xTaskCreatePinnedToCore(
    range_task,
    "range_task",
    2048,
    nullptr,
    1,
    &range_taskh,
    app_cpu
  );
  assert(rc == pdTRUE);
  assert(range_taskh);

   rc = xTaskCreatePinnedToCore(
    sync_task,
    "sync_task",
    2048,
    nullptr,
    1,
    &sync_taskh,
    app_cpu
  );
  assert(rc == pdTRUE);
  assert(sync_taskh);
}

void loop(){
  vTaskDelete(xTaskGetCurrentTaskHandle());
}

/*
Demonstration without using any electronic components, other than the ESP32.
We imagine N philosophers sitting at a round table. Each has a fork to their left and right, for a total of N forks on the table. Philosophers are usually thinking, but they can get hungry and want to eat: to do so, they first grab the fork to their left, if it is free, then the one to their right. They need two forks to eat spaghetti. Once they are finished, they put their forks back down, making them available to the philosophers sitting next to them. This demonstration illustrates how to avoid a deadlock, in which all the philosophers would be stuck with only one fork in their hand.
*/

#define PREVENT_DEADLOCK 1
#define N 4
#define N_EATERS (N-1)

static QueueHandle_t msgq; // This is the handle whose queue contains all the messages which should be printed.
static SemaphoreHandle_t csem; // Counting semaphore's handle. We will be using it just if PREVENT_DEADLOCK is 1.
static int app_cpu = 0;
static volatile unsigned logno = 0; // It is only useful for keeping track of the number of printed messages.

/*
'enum' allows us to create a new data type whose instances, at compile time, are understood by the compiler according to a numeric value we assign to them.
The use is similar to that of macros, except that:
- the compiler only "understands the true value" for data of a type corresponding to the one created with enum;
- while macros cause code substitution before actual compilation, 'enum', at compile time, ensures that the compiler instantly understands the actual value of the code.
*/
enum State{
  Thinking = 0,
  Hungry,
  Eating
};

static const char* state_name[] = {"Thinking", "Hungry", "Eating"};

struct s_philosopher{
  State state;
  unsigned num;
  TaskHandle_t taskh;
  unsigned seed
};

struct s_message{
  State state;
  unsigned num;
};

static s_philosopher philosophers[N];
static SemaphoreHandle_t forks[N];

// Sending the philosopher's state by queue:
static void send_state(s_philosopher *philo){
  s_message msg;
  BaseType_t rc;

  msg.State = philo->state;
  msg.num = philo->num;
  rc = xQueueSendToBack(msgq, &msg, portMAX_DELAY);
  assert(rc==pdPASS);
}

// Defining the philosopher task's function:
static void philo_task(void* argp){
  s_philosopher* philo = (s_philosopher*)argp;
  SemaphoreHandle_t fork1=0, fork2=0; // Left and right fork.
  BaseType_t rc;

  delay(rand_r(&philo->seed)%5 + 1); // 'rand_r(seedAddress)' is conceptually like 'rand()', except that it is multi-threading friendly.

  for(;;){
    philo->state = Thinking;
    send_state(philo);
    delay(rand_r(&philo->seed)%5 + 1);

    philo->state = Hungry;
    send_state(philo);
    delay(rand_r(&philo->seed)%5 + 1);

    // This if, since it contains '#', is read by the preprocessor: if the condition is met, the code is updated. When the code is compiled, the changes predicted by the # will already be implemented.
    #if PREVENT_DEADLOCK // To avoid a deadlock, a condition in which all tasks are blocked due to semaphores, it is necessary to have a maximum of N-1 hungry philosophers: this way, if all the hungry philosophers tried to take the forks, at least one would not get stuck. This is why we are using a counting semaphore with a depth of N-1.
      rc = xSemaphoreTake(csem, portMAX_DELAY);
      assert(rc==pdPASS);
    #endif

    // Picking forks up:
    fork1 = forks[philo->num];
    fork2 = forks[(philo->num + 1) % N];
    rc = xSemaphoreTake(fork1, portMAX_DELAY);
    assert(rc==pdPASS);
    delay(rand_r(&philo->seed)%5 + 1);
    rc = xSemaphoreTake(fork2, portMAX_DELAY);
    assert(rc==pdPASS);

    philo->state = Eating;
    send_state(philo);
    delay(rand_r(&philo->seed)%5 + 1);

    // Putting forks down:
    rc = xSemaphoreGive(fork1);
    assert(rc==pdPASS);
    delay(1);
    rc = xSemaphore(fork2);
    assert(rc==pdPASS);

    #if PREVENT_DEADLOCK
      rc = xSemaphoreGive(csem);
      assert(rc == pdPASS);
    #endif
  }
}

void setup(){
  BaseType_t rc;

  app_cpu = xPortGetCoreID();
  msgq = xQueueCreate(30, sizeof(s_message));
  assert(msgq);

  for (unsigned x=0; x<N; x++){
    forks[x] = xSemaphoreCreateBinary();
    assert(forks[x]);
    rc = xSemaphoreGive(forks[x]); // If it stays empty, no task could ever pick up a fork in the very beginning.
    assert(rc==pdPASS);
    assert(forks[x]);
  }

  delay(2000); // Allowing USB to connect.

  printf("\nThe Dining Philosopher's Problem:\n");
  printf("There are %u Philosophers.\n", N);
  #if PREVENT_DEADLOCK
    csem = xSemaphoreCreateCounting(N_EATERS, N_EATERS); // 'xSemaphoreCreateCounting(uxMaxCount, uxInitialCount)'.
    assert(csem);
    printf("With deadlock prevention.\n");
  #else
    csem = nullptr;
    printf("Without deadlock prevention.\n");
  #endif
    
  // Initializing each philosopher's information:
  for (unsigned x=0; x<N; x++){
    philosophers[x].num = x;
    philosophers[x].seed = hallRead(); // 'hallRead()' is similar to srand(time(NULL)), except for the fact it works with magnetic field fluctuations through ESP32's hardware.
    philosophers[x].state = Thinking;
  }

  // Creating each philosopher's task:
  for (unsigned x=0; x<N; x++){
    rc = xTaskCreatePinnedToCore(
      philo_task,
      "philotsk",
      5000,
      &philosophers[x],
      1,
      &philosophers[x].taskh,
      app_cpu
    );
    assert(rc==pdPASS);
    assert(philosophers[x].taskh);
  }
}

static BaseType_t rc;
void loop(){
  s_message msg;
  while (xQueueReceive(msgq, &msg, 1) == pdPASS){
    printf("%05u: Philosopher %u is %s.", ++logno, msg.num, state_name[msg.state]); // '%05u' is a format specifier for printing unsigned integer numbers that, in our decimal system, appear as five digits. If not all five digits are used, the unused digits are padded with zeros on the left. If the number has more than 5 digits, it is printed in full. We should remember that 'msg.state' is an unsigned integer.
  }
  delay(1);
}