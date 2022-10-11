#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define LED0 15
#define LED1 4

static void Task1code( void * param)
{ 
    while (1)
    {    
        printf("\n Task1 running on core : %d", xPortGetCoreID());
        gpio_set_level(LED0, 0);
        vTaskDelay(1400 / portTICK_PERIOD_MS);
        gpio_set_level(LED0, 1);
        vTaskDelay(1400 / portTICK_PERIOD_MS);
	}
}
    
static void Task2code( void * param)
{ 
    while (1)
    {
        printf("\n Task2 running on core : %d", xPortGetCoreID());
        gpio_set_level(LED1, 0);
        vTaskDelay(700 / portTICK_PERIOD_MS);
        gpio_set_level(LED1, 1); 
        vTaskDelay(700 / portTICK_PERIOD_MS);
	}
}
    

void app_main(void)
{
    gpio_pad_select_gpio(LED0);
    gpio_set_direction(LED0, GPIO_MODE_OUTPUT);
     
    gpio_pad_select_gpio(LED1);
    gpio_set_direction(LED1, GPIO_MODE_OUTPUT);

    //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
    xTaskCreatePinnedToCore(
                    Task1code,   /* Task function. */
                    "Task1",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    NULL,      /* Task handle to keep track of created task */
                    0);         /* pin task to core 0 */
    
    
    
    //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
    xTaskCreatePinnedToCore(
                    Task2code,   /* Task function. */
                    "Task2",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    NULL,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */
}