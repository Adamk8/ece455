/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved

	ECE 455 Lab Project 2 
	Adam Kwan
	V00887099
*/

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "stm32f4_discovery.h"
/* Kernel includes. */
#include "stm32f4xx.h"
#include "../FreeRTOS_Source/include/FreeRTOS.h"
#include "../FreeRTOS_Source/include/queue.h"
#include "../FreeRTOS_Source/include/semphr.h"
#include "../FreeRTOS_Source/include/task.h"
#include "../FreeRTOS_Source/include/timers.h"



struct dd_task {
	TaskHandle_t t_handle;
	uint32_t task_type;
	uint32_t task_id;
	uint32_t execution_time;
	uint32_t release_time;
	uint32_t absolute_deadline;
	uint32_t completion_time;
};

struct dd_task_list {
	struct dd_task task;
	struct dd_task_list *next_task;
};


/*-----------------------------------------------------------*/

/* DD tasks and functions */

void release_dd_task(uint32_t task_type, uint32_t task_id, uint32_t execution_time, uint32_t absolute_deadline);
void complete_dd_task(TaskHandle_t t_handle);
void get_active_dd_task_list(void);
void get_completed_dd_task_list(void);
void get_overdue_dd_task_list(void);

/* F-Tasks */
void xTask_DDS(void *pvParameters);
void xTask_userDefinedTask(void *pvParameters);
void xTask_taskGenerator(void *pvParameters);
void xTask_taskMonitor(void *pvParameters);

/*Helper functions*/
void listAdd(struct dd_task_list *head, struct dd_task next_task);
void listPriorityAdd(struct dd_task_list *head, struct dd_task next_task);
void listRemove(struct dd_task_list *head, uint32_t task_id);


xQueueHandle release_queue = 0;
xQueueHandle release_task_message = 0;
xQueueHandle complete_queue = 0;
xQueueHandle complete_task_message = 0;
xQueueHandle active_task_list = 0;
xQueueHandle active_list_message = 0;
xQueueHandle complete_task_list = 0;
xQueueHandle complete_list_message = 0;
xQueueHandle overdue_task_list = 0;
xQueueHandle overdue_list_message = 0;

enum task_type {PERIODIC,APERIODIC};

/*---------------------------------------------------------*/

int main(void)
{
	SystemInit();

	/* Start Tasks */
	xTaskCreate(xTask_DDS, "DDS", configMINIMAL_STACK_SIZE,NULL,4,NULL);
	xTaskCreate(xTask_taskGenerator, "Generator", configMINIMAL_STACK_SIZE,NULL,3,NULL);
	xTaskCreate(xTask_taskMonitor, "Monitor", configMINIMAL_STACK_SIZE,NULL,1,NULL);

	/* Start the Scheduler */
	vTaskStartScheduler();

	return 0;
}

/*----------------------- FreeRTOS tasks -------------------------*/
void xTask_DDS(void *pvParameters){
	printf("DDS started\n");
	//Heads of the lists
	struct dd_task_list *active_list_head = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	active_list_head->next_task = NULL;

	struct dd_task_list *complete_list_head = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	complete_list_head->next_task = NULL;

	struct dd_task_list *overdue_list_head = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	overdue_list_head->next_task = NULL;


	TaskHandle_t currently_active = NULL;
	struct dd_task next_task;
	TaskHandle_t finished_task;
	uint16_t message;
	while(1){
		//For New Tasks

		if(release_task_message != 0 && release_queue != 0 && xQueueReceive(release_queue, &next_task, 30)){
			//printf("Releasing Task %d\n", (int)next_task.task_id);
			next_task.release_time = (uint32_t)xTaskGetTickCount();
			listPriorityAdd(active_list_head, next_task);
			uint16_t reply = 1;
			if(!xQueueSend(release_task_message, &reply,30))printf("Failed to send success message to release\n");
			//vTaskDelay(1);
			// Set priorities
			if (currently_active != NULL && currently_active != active_list_head->next_task->task.t_handle){
				vTaskPrioritySet(currently_active, 1);
			}
			currently_active = active_list_head->next_task->task.t_handle;
			vTaskPrioritySet(currently_active, 2);
		}

		//For Finished tasks
		if(complete_task_message != 0 && complete_queue != 0 && xQueueReceive(complete_queue, &finished_task, 30)){

			struct dd_task_list *curr = active_list_head;
			uint16_t found = 1;
			while(curr->task.t_handle != finished_task){
				if (curr->next_task == NULL){
					found = 0;
					printf("Task to complete not found\n");
				} else {
					curr = curr->next_task;
				}

			}
			if (found){
				//printf("Completing Task %d\n", (int)curr->task.task_id);
				curr->task.completion_time = (uint32_t) xTaskGetTickCount();
				if (curr->task.completion_time > curr->task.absolute_deadline){
					listAdd(overdue_list_head, curr->task);
				} else {
					listAdd(complete_list_head, curr->task);
				}
				listRemove(active_list_head, curr->task.task_id);
				free(curr);
			}
			uint32_t reply = 1;
			if(!xQueueOverwrite(complete_task_message, &reply))printf("Failed to send success message to complete\n");
			//vTaskDelay(pdMS_TO_TICKS(1));
			if(active_list_head->next_task != NULL){
				if (currently_active != NULL && currently_active != active_list_head->next_task->task.t_handle){
					vTaskPrioritySet(currently_active, 1);
				}
				currently_active = active_list_head->next_task->task.t_handle;
				vTaskPrioritySet(currently_active, 2);
			}
		}

		//Active task list request
		message = 0;
		if(active_list_message != 0 && xQueueReceive(active_list_message, &message, 30)){
			if(message){
				int reply = 2;
				if(!xQueueOverwrite(active_task_list, &active_list_head))printf("Failed to send active task list\n");
				if(!xQueueOverwrite(active_list_message, &reply))printf("Failed to send active_list_message\n");
			}
		}

		//Complete task list request
		message = 0;
		if (complete_list_message != 0 && xQueueReceive(complete_list_message, &message, 30)){
			if(message){
				int reply = 2;
				if(!xQueueOverwrite(complete_task_list, &complete_list_head))printf("Failed to send active task list\n");
				if(!xQueueOverwrite(complete_list_message, &reply))printf("Failed to send complete message\n");
			}
		}

		//Overdue task list request
		message = 0;
		if (overdue_list_message != 0 && xQueueReceive(overdue_list_message, &message, 30)){
			if(message){
				int reply = 2;
				if(!xQueueOverwrite(overdue_task_list, &overdue_list_head))printf("Failed to send overdue task list\n");
				if(!xQueueOverwrite(overdue_list_message, &reply))printf("Failed to send overdue message\n");
			}
		}

		vTaskDelay(pdMS_TO_TICKS(1));

	}

}

void xTask_taskGenerator(void *pvParameters){
	uint32_t i = 0;
	uint32_t test_bench = 1;
	uint32_t hyper_period;
	while (1){
// 		Test Bench 1
		if (test_bench == 1){
			hyper_period = 1500;
			release_dd_task(0, 1, 95, 500 + i*hyper_period);
			release_dd_task(0, 2, 150, 500 + i*hyper_period);
			release_dd_task(0, 3, 250, 750 + i*hyper_period);
			vTaskDelay(pdMS_TO_TICKS(500));

			release_dd_task(0, 1, 95, 1000 + i*hyper_period);
			release_dd_task(0, 2, 150, 1000 + i*hyper_period);
			vTaskDelay(pdMS_TO_TICKS(250));

			release_dd_task(0, 3, 250, 1500 + i*hyper_period);
			vTaskDelay(pdMS_TO_TICKS(250));

			release_dd_task(0, 1, 95, 1500 + i*hyper_period);
			release_dd_task(0, 2, 150, 1500 + i*hyper_period);
			vTaskDelay(pdMS_TO_TICKS(500));
		}

//		Test Bench 2
		if (test_bench == 2){
			hyper_period = 1500;
			release_dd_task(0, 1, 95, 250 + i*hyper_period);
			release_dd_task(0, 2, 150, 500 + i*hyper_period);
			release_dd_task(0, 3, 250, 750 + i*hyper_period);
			vTaskDelay(pdMS_TO_TICKS(250));

			release_dd_task(0, 1, 95, 500 + i*hyper_period);
			vTaskDelay(pdMS_TO_TICKS(250));

			release_dd_task(0, 1, 95, 750 + i*hyper_period);
			release_dd_task(0, 2, 150, 1000 + i*hyper_period);
			vTaskDelay(pdMS_TO_TICKS(250));

			release_dd_task(0, 1, 95, 1000 + i*hyper_period);
			release_dd_task(0, 3, 250, 1500 + i*hyper_period);
			vTaskDelay(pdMS_TO_TICKS(250));

			release_dd_task(0, 1, 95, 1250 + i*hyper_period);
			release_dd_task(0, 2, 150, 1500 + i*hyper_period);
			vTaskDelay(pdMS_TO_TICKS(250));

			release_dd_task(0, 1, 95, 1500 + i*hyper_period);
			vTaskDelay(pdMS_TO_TICKS(250));
		}

		//Test Bench 3
		if (test_bench == 3){
			hyper_period = 500;
			release_dd_task(0, 1, 100, 500 + i*hyper_period);
			release_dd_task(0, 2, 200, 500 + i*hyper_period);
			release_dd_task(0, 2, 200, 500 + i*hyper_period);
			vTaskDelay(pdMS_TO_TICKS(500));
		}

		i++;
	}
}

void xTask_taskMonitor(void *pvParameters){
	struct dd_task_list *active_tasks = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
 	struct dd_task_list *complete_tasks = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
 	struct dd_task_list *overdue_tasks = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	while(1){
		vTaskDelay(pdMS_TO_TICKS(1600));
		printf("\n---------- Active Tasks ----------\n\n");
		get_active_dd_task_list();
		if(xQueueReceive(active_task_list, &active_tasks, 100)){
			//If list not empty
			if (active_tasks->next_task != NULL ){
				active_tasks = active_tasks->next_task;
				while(active_tasks->next_task != NULL){
					printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d\n", (int)active_tasks->task.task_type,(int)active_tasks->task.task_id, (int)active_tasks->task.absolute_deadline, (int)active_tasks->task.release_time);
					active_tasks = active_tasks->next_task;
				}
				printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d\n", (int)active_tasks->task.task_type,(int)active_tasks->task.task_id, (int)active_tasks->task.absolute_deadline, (int)active_tasks->task.release_time);
			}
			free(active_tasks);
			vQueueDelete(active_task_list);
			active_task_list = 0;
		}
		printf("\n---------- Complete Tasks ----------\n\n");
		get_completed_dd_task_list();
		if(xQueueReceive(complete_task_list, &complete_tasks, 100)){
			//If list not empty
			if (complete_tasks->next_task != NULL ){
				complete_tasks= complete_tasks->next_task;
				while(complete_tasks->next_task != NULL){
					printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d Completion Time: %d\n", (int)complete_tasks->task.task_type,(int)complete_tasks->task.task_id, (int)complete_tasks->task.absolute_deadline, (int)complete_tasks->task.release_time, (int)complete_tasks->task.completion_time);
					complete_tasks = complete_tasks->next_task;
				}
				printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d Completion Time: %d\n", (int)complete_tasks->task.task_type,(int)complete_tasks->task.task_id, (int)complete_tasks->task.absolute_deadline, (int)complete_tasks->task.release_time, (int)complete_tasks->task.completion_time);
			}
			free(complete_tasks);
			vQueueDelete(complete_task_list);
			complete_task_list = 0;
		}

		printf("\n---------- Overdue Tasks ----------\n\n");
		get_overdue_dd_task_list();
		if(xQueueReceive(overdue_task_list, &overdue_tasks, 100)){
			//If list not empty
			if (overdue_tasks->next_task != NULL ){
				overdue_tasks= overdue_tasks->next_task;
				while(overdue_tasks->next_task != NULL){
					printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d Completion Time: %d\n", (int)overdue_tasks->task.task_type,(int)overdue_tasks->task.task_id, (int)overdue_tasks->task.absolute_deadline, (int)overdue_tasks->task.release_time, (int)overdue_tasks->task.completion_time);
					overdue_tasks = overdue_tasks->next_task;
				}
				printf("Task: Type: %d ID: %d Deadline: %d Release Time: %d Completion Time: %d\n", (int)overdue_tasks->task.task_type,(int)overdue_tasks->task.task_id, (int)overdue_tasks->task.absolute_deadline, (int)overdue_tasks->task.release_time, (int)overdue_tasks->task.completion_time);
			}
			free(overdue_tasks);

			vQueueDelete(overdue_task_list);
			overdue_task_list = 0;
		}
	}
}

void xTask_userDefinedTask(void *pvParameters){
	while (uxTaskPriorityGet(NULL) < 2);
	int start = (int)xTaskGetTickCount();
	int end = start + (int) pvParameters;
	while ((int)xTaskGetTickCount() < end);

	vTaskPrioritySet(NULL, 3);
	complete_dd_task(xTaskGetCurrentTaskHandle());
	vTaskSuspend(NULL);
}

/*----------------------- DDS tasks -------------------------*/

void release_dd_task(uint32_t task_type, uint32_t task_id, uint32_t execution_time, uint32_t absolute_deadline){
	// Create new task
	//printf("Releasing Function %d\n", (int)task_id);
	struct dd_task new_task;
	new_task.task_type = task_type;
	new_task.task_id = task_id;
	new_task.absolute_deadline = absolute_deadline;
	new_task.execution_time = execution_time;
	new_task.message = 0;
	xTaskCreate(xTask_userDefinedTask,"UDT",configMINIMAL_STACK_SIZE,(void *) new_task.execution_time,1,&(new_task.t_handle));

	// Create queue
	release_queue = xQueueCreate(1,sizeof(struct dd_task));
	vQueueAddToRegistry(release_queue,"Release");
	release_task_message = xQueueCreate(1,sizeof(int));
	vQueueAddToRegistry(release_task_message,"Release message");

	// Send message
	if(!xQueueSend(release_queue, &new_task, 30))printf("Failed to release new task\n");

	// Delete queue
	int reply;
	while(1){
		if(xQueueReceive(release_task_message, &reply, 30)){
			break;
		}
	}
	vQueueDelete(release_task_message);
	vQueueDelete(release_queue);
	release_queue = 0;
	release_task_message = 0;
}

void complete_dd_task(TaskHandle_t t_handle){
		// Create queue
		complete_queue = xQueueCreate(1,sizeof(TaskHandle_t));
		vQueueAddToRegistry(complete_queue,"Delete");
		complete_task_message = xQueueCreate(1,sizeof(int));
		vQueueAddToRegistry(complete_task_message,"Delete message");

		// Send message
		if(!xQueueOverwrite(complete_queue, &t_handle))printf("Failed to send complete message\n");

		// Delete queue
		int reply;
		while(1){
			if(xQueueReceive(complete_task_message, &reply, 30)){
				break;
			}
		}
		vTaskDelete(t_handle);
		vQueueDelete(complete_task_message);
		vQueueDelete(complete_queue);
		complete_task_message = 0;
		complete_queue = 0;
}

void get_active_dd_task_list(void){
	// Create Queues
	active_task_list = xQueueCreate(1, sizeof(struct dd_task_list));
	vQueueAddToRegistry(active_task_list,"Active");
	active_list_message = xQueueCreate(1, sizeof(int));
	vQueueAddToRegistry(active_list_message,"Active message");
	int message = 1;
	int reply;

	//Send Message
	if(!xQueueOverwrite(active_list_message, &message))printf("Failed to send active task message\n");

	//Wait For Reply
	while(1){
		if(xQueuePeek(active_list_message, &reply, 30)){
			if (reply == 2) break;
		}
	}
	vQueueDelete(active_list_message);
	active_list_message = 0;
}

void get_completed_dd_task_list(void){
	// Create Queues
	complete_task_list = xQueueCreate(1, sizeof(struct dd_task_list));
	vQueueAddToRegistry(complete_task_list,"Complete");
	complete_list_message = xQueueCreate(1, sizeof(int));
	vQueueAddToRegistry(complete_task_message,"Complete message");

	int message = 1;
	int reply;
	//Send Message
	if(!xQueueOverwrite(complete_list_message, &message))printf("Failed to send complete task message\n");
	//Wait For Reply
	while(1){
		if(xQueuePeek(complete_list_message, &reply, 30)){
			if (reply == 2) break;
		}
	}
	vQueueDelete(complete_list_message);
	complete_list_message = 0;
}

void get_overdue_dd_task_list(void){
	// Create Queues
	overdue_task_list = xQueueCreate(1, sizeof(struct dd_task_list));
	vQueueAddToRegistry(overdue_task_list,"Overdue");
	overdue_list_message = xQueueCreate(1, sizeof(int));
	vQueueAddToRegistry(overdue_list_message,"Overdue message");

	int message = 1;
	int reply;
	//Send Message
	if(!xQueueOverwrite(overdue_list_message, &message))printf("Failed to send complete task message\n");
	//Wait For Reply
	while(1){
		if(xQueuePeek(overdue_list_message, &reply, 30)){
			if (reply == 2) break;
		}
	}
	vQueueDelete(overdue_list_message);
	overdue_list_message = 0;
}

/*----------------------- Helper Functions -------------------------*/

void listAdd(struct dd_task_list *head, struct dd_task next_task){
	struct dd_task_list *new_node = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	new_node->task = next_task;
	new_node->next_task = NULL;
	struct dd_task_list *curr = head;
	while (curr->next_task != NULL){
		curr = curr->next_task;
	}
	curr->next_task = new_node;
}

void listPriorityAdd(struct dd_task_list *head, struct dd_task next_task){
	struct dd_task_list *new_node = (struct dd_task_list*)malloc(sizeof(struct dd_task_list));
	new_node->task = next_task;
	struct dd_task_list *curr = head;
	while (curr->next_task != NULL){
		if(curr->next_task->task.absolute_deadline > new_node->task.absolute_deadline){
			new_node->next_task = curr->next_task;
			curr->next_task = new_node;
			return;
		}
		curr = curr->next_task;
	}
	new_node->next_task = curr->next_task;
	curr->next_task = new_node;
}

void listRemove(struct dd_task_list *head, uint32_t task_id){
	struct dd_task_list *curr = head;
	struct dd_task_list *prev = head;
	while (curr->task.task_id != task_id){
		if(curr->next_task == NULL){
			printf("ID not found\n");
			return;
		}
		prev = curr;
		curr = curr->next_task;
	}
	if (curr->next_task == NULL){
		prev->next_task = NULL;
	} else {
		prev->next_task = curr->next_task;
	}
	free(curr);
}

/*-----------------------------------------------------------*/



/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* The malloc failed hook is enabled by setting
	configUSE_MALLOC_FAILED_HOOK to 1 in FreeRTOSConfig.h.

	Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	for( ;; );
}

/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( xTaskHandle pxTask, signed char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configconfigCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected.  pxCurrentTCB can be
	inspected in the debugger if the task name passed into this function is
	corrupt. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
volatile size_t xFreeStackSpace;

	/* The idle task hook is enabled by setting configUSE_IDLE_HOOK to 1 in
	FreeRTOSConfig.h.

	This function is called on each cycle of the idle task.  In this case it
	does nothing useful, other than report the amount of FreeRTOS heap that
	remains unallocated. */
	xFreeStackSpace = xPortGetFreeHeapSize();

	if( xFreeStackSpace > 100 )
	{
		/* By now, the kernel has allocated everything it is going to, so
		if there is a lot of heap remaining unallocated then
		the value of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h can be
		reduced accordingly. */
	}
}
/*-----------------------------------------------------------*/

