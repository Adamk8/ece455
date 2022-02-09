/*
    FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.

    ***************************************************************************
    >>!   NOTE: The modification to the GPL is included to allow you to     !<<
    >>!   distribute a combined work that includes FreeRTOS without being   !<<
    >>!   obliged to provide the source code for proprietary components     !<<
    >>!   outside of the FreeRTOS kernel.                                   !<<
    ***************************************************************************

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available on the following
    link: http://www.freertos.org/a00114.html

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that is more than just the market leader, it     *
     *    is the industry's de facto standard.                               *
     *                                                                       *
     *    Help yourself get started quickly while simultaneously helping     *
     *    to support the FreeRTOS project by purchasing a FreeRTOS           *
     *    tutorial book, reference manual, or both:                          *
     *    http://www.FreeRTOS.org/Documentation                              *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
    the FAQ page "My application does not run, what could be wwrong?".  Have you
    defined configASSERT()?

    http://www.FreeRTOS.org/support - In return for receiving this top quality
    embedded software for free we request you assist our global community by
    participating in the support forum.

    http://www.FreeRTOS.org/training - Investing in training allows your team to
    be as productive as possible as early as possible.  Now you can receive
    FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
    Ltd, and the world's leading authority on the world's leading RTOS.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
    Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

    http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
    Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and commercial middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
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



#define RED 	0
#define YELLOW	1
#define GREEN	2

#define LOW 	0
#define LOWMED	1
#define MED		2
#define MEDHIGH	3
#define HIGH	4

#define TOTAL_CARS 19

/*-----------------------------------------------------------*/

void init_GPIOC(void);
void init_ADC(void);
void xTask_TrafficFlowAdjustment(void *pvParameters);
void xTask_TrafficGenerator(void *pvParameters);
void xTask_TrafficLightState(void *pvParameters);
void xTask_SystemDisplay(void *pvParameters);
void vCallbackfunction(TimerHandle_t xTimer);

void resetTraffic();
void clockShift();
void setOne();
void setZero();
int randomNumber(int);
void shiftCars(int *,int);
void printCars(int *);
void shiftRedLight(int *, int);


TimerHandle_t myTimer;
xQueueHandle trafficFlow = 0;
xQueueHandle lightState = 0;
xQueueHandle nextCar = 0;

/*-----------------------------------------------------------*/

int main(void)
{
	SystemInit();
	/* Initialize GPIOC and ADC */
	init_GPIOC();
	printf("GPIOC Init Done\n");
	init_ADC();
	printf("ADC Init Done\n");

	myTimer = xTimerCreate("Timer", pdMS_TO_TICKS(1000), pdFALSE, (void *)0, vCallbackfunction);
	if (myTimer == NULL){
		printf("Timer Not created\n");
	}

	/* Create Queues for traffic flow and light states */
	trafficFlow = xQueueCreate(100, sizeof(int));
	lightState = xQueueCreate(100, sizeof(int));
	nextCar = xQueueCreate(100, sizeof(int));
	vQueueAddToRegistry(trafficFlow,"Flow queue");
	vQueueAddToRegistry(lightState,"Light queue");
	vQueueAddToRegistry(nextCar,"Car queue");


	int red = RED;
	if(!xQueueSend(lightState, &red, 1000)){
		printf("Error sending to queue.\n");
	}

	/* Start Tasks */
	xTaskCreate(xTask_TrafficFlowAdjustment, "Flow", configMINIMAL_STACK_SIZE,NULL,1,NULL);
	xTaskCreate(xTask_TrafficGenerator, "Generator", configMINIMAL_STACK_SIZE,NULL,1,NULL);
	xTaskCreate(xTask_TrafficLightState, "Lights", configMINIMAL_STACK_SIZE,NULL,2,NULL);
	xTaskCreate(xTask_SystemDisplay, "Display", configMINIMAL_STACK_SIZE,NULL,3,NULL);



	/* Start the Scheduler */
	vTaskStartScheduler();

	return 0;
}

void init_GPIOC(void){
		RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC,ENABLE);

		GPIO_InitTypeDef GPIO_InitStruct_01268;
		GPIO_InitStruct_01268.GPIO_Mode = GPIO_Mode_OUT;
		GPIO_InitStruct_01268.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_8 | GPIO_Pin_6 | GPIO_Pin_7;
		GPIO_InitStruct_01268.GPIO_OType = GPIO_OType_PP;
		GPIO_InitStruct_01268.GPIO_PuPd = GPIO_PuPd_NOPULL;
		GPIO_InitStruct_01268.GPIO_Speed = GPIO_Speed_2MHz;
		GPIO_Init(GPIOC, &GPIO_InitStruct_01268);

		GPIO_InitTypeDef GPIO_InitStruct_3;
		GPIO_InitStruct_3.GPIO_Mode = GPIO_Mode_AN;
		GPIO_InitStruct_3.GPIO_Pin = GPIO_Pin_3;
		GPIO_InitStruct_3.GPIO_OType = GPIO_OType_OD;
		GPIO_InitStruct_3.GPIO_PuPd = GPIO_PuPd_NOPULL;
		GPIO_Init(GPIOC, &GPIO_InitStruct_3);

		GPIO_ResetBits(GPIOC,GPIO_Pin_0);
		GPIO_ResetBits(GPIOC,GPIO_Pin_1);
		GPIO_ResetBits(GPIOC,GPIO_Pin_2);
		resetTraffic();

}
/*-----------------------------------------------------------*/

void init_ADC(void){
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1,ENABLE);
	ADC_InitTypeDef ADC_InitStruct;

	ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStruct.ADC_ScanConvMode = DISABLE;
	ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Left;
	ADC_InitStruct.ADC_Resolution = ADC_Resolution_8b;
	ADC_InitStruct.ADC_ExternalTrigConv = DISABLE;

	ADC_Init(ADC1,&ADC_InitStruct);
	ADC_Cmd(ADC1,ENABLE);
	ADC_RegularChannelConfig(ADC1,ADC_Channel_13,1,ADC_SampleTime_3Cycles);
}


/*-----------------------------------------------------------*/

/* Change the flow of traffic based on potentiometer reading, light or heavy traffic, sent to Traffic Generator and light state tasks */
void xTask_TrafficFlowAdjustment(void *pvParameters){
	int flow_to_send = LOW;
	while(1){
		ADC_SoftwareStartConv(ADC1);
		while(!ADC_GetFlagStatus(ADC1,ADC_FLAG_EOC));

		int flow_value = ADC_GetConversionValue(ADC1);
		if (flow_value <= 12000){
			flow_to_send = LOW;
		} else if (flow_value > 12000 && flow_value <= 25000){
			flow_to_send = LOWMED;
		} else if (flow_value > 25000 && flow_value <= 38000){
			flow_to_send = MED;
		} else if (flow_value > 38000 && flow_value <= 51000){
			flow_to_send = MEDHIGH;
		} else if (flow_value > 51000){
			flow_to_send = HIGH;
		}

		if (!xQueueSend(trafficFlow, &flow_to_send, 500)){
			printf("Failed to Send FLow to queue\n");
		}
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}
/* Generate the traffic base on the flow from TrafficFlowAdjustment, sent to display task after */
void xTask_TrafficGenerator(void *pvParameters){
	int flow;
	while(1){
		if(xQueueReceive(trafficFlow, &flow, 500)){
			int car = 0;
			switch(flow)
			{
				case LOW:
					//1/6 chance of a car
					if(randomNumber(6) == 0)car = 1;
					else {car = 0;}
					if(!xQueueSend(nextCar, &car, 500))printf("Failed to update next car\n");
					break;
				case LOWMED:
					//1/4 chance of a car
					if(randomNumber(4) == 0)car = 1;
					else {car = 0;}
					if(!xQueueSend(nextCar, &car, 500))printf("Failed to update next car\n");
					break;
				case MED:
					//1/3 chance of a car
					if(randomNumber(3) == 0)car = 1;
					else {car = 0;}
					if(!xQueueSend(nextCar, &car, 500))printf("Failed to update next car\n");
					break;
				case MEDHIGH:
					//1/2 chance of a car
					if(randomNumber(2) == 0)car = 1;
					else {car = 0;}
					if(!xQueueSend(nextCar, &car, 500))printf("Failed to update next car\n");
					break;
				case HIGH:
					//90% chance of a car
					if(randomNumber(10) != 0)car = 1;
					else {car = 0;}
					if(!xQueueSend(nextCar, &car, 500))printf("Failed to update next car\n");
					break;
				default:
					printf("Not a valid Flow\n");
			}
		}
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}


/* Change the light state based of the load found in TrafficFlowAdjustment */
void xTask_TrafficLightState(void *pvParameters){
	int flow,light;
	while(1){

		if(xQueuePeek(trafficFlow, &flow, 500)){
			int green_time = 8;
			switch(flow)
			{
				case LOW:
					green_time = 5000;
					break;
				case LOWMED:
					green_time = 7000;
					break;
				case MED:
					green_time = 8000;
					break;
				case MEDHIGH:
					green_time = 9000;
					break;
				case HIGH:
					green_time = 10000;
					break;
				default:
					printf("Not a valid Flow\n");
			}
			if(xQueuePeek(lightState, &light, 500)){
				printf("Changing Light state.\n");
				switch(light)
				{
					/* If red then green next*/
					case RED:
						if (xTimerStart(myTimer,pdMS_TO_TICKS(green_time)) != pdPASS){
							printf("Timer Failed to start\n");
						}
						vTaskDelay(green_time);
						break;
					/* If green then yellow next*/
					case GREEN:
						if (xTimerStart(myTimer,pdMS_TO_TICKS(1000)) != pdPASS){
							printf("Timer Failed to start\n");
						}
						vTaskDelay(1000);
						break;
					/*If yellow then red next*/
					case YELLOW:
						if (xTimerStart(myTimer,pdMS_TO_TICKS(15000 - green_time)) != pdPASS){
							printf("Timer Failed to start\n");
						}
						vTaskDelay(16000 - green_time);
						break;
					default:
						printf("Invalid Light state.\n");
				}
			}
		}
	}
}

/* Visualize the traffic and light states based on Traffic Generator and TrafficLightState */
void xTask_SystemDisplay(void *pvParameters){
	int car,light;
	int cars[TOTAL_CARS] = {0};
	while(1){
		if(xQueuePeek(lightState, &light, 500) && xQueueReceive(nextCar, &car, 500)){
			switch(light)
				{
					case RED:
						//Yellow off
						GPIO_ResetBits(GPIOC,GPIO_Pin_1);
						//Red on
						GPIO_SetBits(GPIOC,GPIO_Pin_0);
						//Red light car shift
						shiftRedLight(cars, car);
						break;
					case GREEN:
						//Red off
						GPIO_ResetBits(GPIOC,GPIO_Pin_0);
						//Green on
						GPIO_SetBits(GPIOC,GPIO_Pin_2);
						//Green light car shift
						shiftCars(cars,car);
						break;
					case YELLOW:
						//Green off
						GPIO_ResetBits(GPIOC,GPIO_Pin_2);
						//Yellow on
						GPIO_SetBits(GPIOC,GPIO_Pin_1);
						//Red light car shift
						shiftRedLight(cars, car);
						break;
					default:
						printf("Invalid Light state.\n");
				}
			for(int i = TOTAL_CARS -1; i >= 0; i--){
				if(cars[i]){
					setOne();
				} else {
					setZero();
				}
			}
		}
		vTaskDelay(pdMS_TO_TICKS(250));
	}
}

/* This is the callback function for the timer, it stops the timer */
void vCallbackfunction(TimerHandle_t xTimer){
	printf("Changing Light state.\n");
	int light;
	if(xQueueReceive(lightState, &light, 500)){
		int next_state;
		switch(light)
		{
			/* If red then green next*/
			case RED:
				next_state = GREEN;
				if(!xQueueSend(lightState, &next_state, 500))printf("Failed to update light state\n");
				break;
			/* If green then yellow next*/
			case GREEN:
				next_state = YELLOW;
				if(!xQueueSend(lightState, &next_state, 500))printf("Failed to update light state\n");
				break;
			/*If yellow then red next*/
			case YELLOW:
				next_state = RED;
				if(!xQueueSend(lightState, &next_state, 500))printf("Failed to update light state\n");
				break;
			default:
				printf("Invalid Light state.\n");
		}
	}
}

/* ---------------------- Helper Functions -----------------------*/



void resetTraffic(){
	GPIO_SetBits(GPIOC,GPIO_Pin_8);
	GPIO_ResetBits(GPIOC,GPIO_Pin_8);
	GPIO_SetBits(GPIOC,GPIO_Pin_8);
}
void clockShift(){
	GPIO_SetBits(GPIOC,GPIO_Pin_7);
	GPIO_ResetBits(GPIOC,GPIO_Pin_7);
	GPIO_SetBits(GPIOC,GPIO_Pin_7);
}
void setZero(){
	GPIO_SetBits(GPIOC,GPIO_Pin_8);
	GPIO_ResetBits(GPIOC,GPIO_Pin_6);
	clockShift();
}
void setOne(){
	GPIO_SetBits(GPIOC,GPIO_Pin_8);
	GPIO_SetBits(GPIOC,GPIO_Pin_6);
	clockShift();
}
int randomNumber(int range){
	return rand() % range;
}

void shiftCars(int *array, int input){
	for(int i = (TOTAL_CARS - 1); i >= 0; i--){
		if (i == 0){
			array[i] = input;
		} else {
			array[i] = array [i-1];
			array[i-1] = 0;
		}
	}

}

void shiftRedLight(int *array, int input){
	//Cars in front of the light
	for(int i = 7; i > 0;i--){
		if(array[i] == 0){
			array[i] = array[i-1];
			array[i-1] = 0;
		}
	}
	//if there is still space send the next car
	if (array[0] == 0){
		array[0] = input;
	}
	//Cars Past the light
	for(int i = TOTAL_CARS - 1; i > 7; i--){
		if (i == 8){
			array[i] = 0;
		} else {
			array[i] = array [i-1];
			array[i-1] = 0;
		}

	}

}

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

