#include "main.h"
#include "task.h"
#include "port.h"
#include "sched.h"
#include "sync.h"
#include "queue.h"
#include "stm32f7xx.h"
#include <stddef.h>

TCB_t tasks[NUM_TASKS];
/* 8-byte aligned: the Cortex-M ABI (AAPCS) requires an 8-byte aligned stack */
uint32_t task_stacks[NUM_TASKS][TASK_STACK_SIZE] __attribute__((aligned(8)));

/* ---- Kernel objects ---- */
static semaphore_t modbus_request_sem;   /* given when a Modbus frame arrives */
static mutex_t     sensor_data_mutex;    /* protects sensor_data */
static queue_t     sensor_queue;         /* sensor samples -> Modbus task */
static uint32_t    sensor_queue_buf[SENSOR_QUEUE_LEN];

/* ---- Shared data ---- */
static sensor_data_t sensor_data;

/* ---- Debug counters (watch these in Renode / the debugger) ---- */
volatile uint32_t modbus_requests_handled = 0;
volatile uint32_t modbus_last_sample = 0;
volatile uint32_t sensor_reads = 0;
volatile uint32_t sensor_queue_drops = 0;
volatile uint32_t heartbeat_counter = 0;
volatile uint8_t  led_state = 0;

/*
 * HIGH priority: Modbus handler.
 * Sleeps on a semaphore until a request arrives, then answers it using the
 * latest sensor data. Because it has the highest priority it runs as soon
 * as the semaphore is given.
 */
void modbus_task(void) {
    uint32_t sample;

    while (1) {
        sem_wait(&modbus_request_sem);

        /* Drain queued samples (non-blocking) */
        while (queue_try_receive(&sensor_queue, &sample) == 0) {
            modbus_last_sample = sample;
        }

        mutex_lock(&sensor_data_mutex);
        /* TODO: copy sensor_data into Modbus holding registers and send the reply */
        (void)sensor_data.temperature;
        mutex_unlock(&sensor_data_mutex);

        modbus_requests_handled++;
    }
}

/*
 * MEDIUM priority: Sensor reader.
 * Every SENSOR_PERIOD_TICKS: read the sensor, update the shared struct
 * under the mutex and push the sample into the queue.
 */
void sensor_task(void) {
    while (1) {
        /* TODO: replace with the real sensor driver (I2C read etc.) */
        uint32_t fake_temperature = 200 + (sensor_reads % 50);
        uint32_t fake_humidity    = 400 + (sensor_reads % 20);

        mutex_lock(&sensor_data_mutex);
        sensor_data.temperature = fake_temperature;
        sensor_data.humidity = fake_humidity;
        sensor_data.sample_count++;
        mutex_unlock(&sensor_data_mutex);

        if (queue_try_send(&sensor_queue, fake_temperature) != 0) {
            sensor_queue_drops++;       /* queue full: nobody consumed in time */
        }

        sensor_reads++;
        task_delay(SENSOR_PERIOD_TICKS);
    }
}

/*
 * LOW priority: Heartbeat.
 * Toggles an LED stub and, for testing, simulates an incoming Modbus request.
 */
void heartbeat_task(void) {
    while (1) {
        heartbeat_counter++;
        led_state ^= 1;                 /* TODO: HAL_GPIO_TogglePin(LED_PORT, LED_PIN) */

        /* Stub: simulate a Modbus request. Later the UART RX ISR calls sem_give(). */
        sem_give(&modbus_request_sem);

        task_delay(HEARTBEAT_PERIOD_TICKS);
    }
}

int main(void) {
    scheduler_init();

    sem_init(&modbus_request_sem, 0, 1);
    mutex_init(&sensor_data_mutex);
    queue_init(&sensor_queue, sensor_queue_buf, SENSOR_QUEUE_LEN);

    scheduler_add_task(&tasks[0], 1, PRIO_MODBUS,    modbus_task,    task_stacks[0], TASK_STACK_SIZE);
    scheduler_add_task(&tasks[1], 2, PRIO_SENSOR,    sensor_task,    task_stacks[1], TASK_STACK_SIZE);
    scheduler_add_task(&tasks[2], 3, PRIO_HEARTBEAT, heartbeat_task, task_stacks[2], TASK_STACK_SIZE);

    /*
     * PendSV and SysTick at the SAME, lowest priority so they can never
     * preempt each other (a SysTick in the middle of PendSV could change
     * next_task while a context switch is half done).
     * NVIC_SetPriority takes the 4-bit level directly: 15 = lowest on STM32F7.
     */
    NVIC_SetPriority(PendSV_IRQn,  (1UL << __NVIC_PRIO_BITS) - 1UL);
    NVIC_SetPriority(SysTick_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL);
    SysTick_Config(SystemCoreClock / 1000); // 1ms tick

    scheduler_start();

    while (1) { } // never reached
}


void Error_Handler(void) {
    __disable_irq();
    while (1) { }
}
