/*
    FreeRTOS V6.1.1 - Copyright (C) 2011 Real Time Engineers Ltd.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation AND MODIFIED BY the FreeRTOS exception.
    ***NOTE*** The exception to the GPL is included to allow you to distribute
    a combined work that includes FreeRTOS without being obliged to provide the
    source code for proprietary components outside of the FreeRTOS kernel.
    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
    more details. You should have received a copy of the GNU General Public
    License and the FreeRTOS license exception along with FreeRTOS; if not it
    can be viewed here: http://www.freertos.org/a00114.html and also obtained
    by writing to Richard Barry, contact details for whom are available on the
    FreeRTOS WEB site.

    1 tab == 4 spaces!

    http://www.FreeRTOS.org - Documentation, latest information, license and
    contact details.

    http://www.SafeRTOS.com - A version that is certified for use in safety
    critical systems.

    http://www.OpenRTOS.com - Commercial support, development, porting,
    licensing and training services.
*/

/* FreeRTOS.org includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "sensor.h"
#include "lpc17xx_pinsel.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_timer.h"
#include "oled.h"
#include "acc.h"
#include "rgb.h"
#include "light.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "basic_io.h"

static void intToString(int, uint8_t*, uint32_t, uint32_t);
/* Demo includes. */
#include "basic_io.h"
static uint8_t buf[10];
/* The task functions. */
static void vSenderTask( void *pvParameters );
static void vReceiverTask( void *pvParameters );
//void vContinuousProcessingTask( void *pvParameters );
//void vPeriodicTask( void *pvParameters );
xQueueHandle xQueue;


/* Define the strings that will be passed in as the task parameters.  These are
defined const and off the stack to ensure they remain valid when the tasks are
executing. */
//const char *pcTextForTask1 = "Continuous task 1 running\n";
//const char *pcTextForTask2 = "Continuous task 2 running\n";
//const char *pcTextForPeriodicTask = "Periodic task is running\n";

/*-----------------------------------------------------------*/

static void init_ssp(void)
{
	SSP_CFG_Type SSP_ConfigStruct;
	PINSEL_CFG_Type PinCfg;

	/*
	 * Initialize SPI pin connect
	 * P0.7 - SCK;
	 * P0.8 - MISO
	 * P0.9 - MOSI
	 * P2.2 - SSEL - used as GPIO
	 */
	PinCfg.Funcnum = 2;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Portnum = 0;
	PinCfg.Pinnum = 7;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 8;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 9;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Funcnum = 0;
	PinCfg.Portnum = 2;
	PinCfg.Pinnum = 2;
	PINSEL_ConfigPin(&PinCfg);

	SSP_ConfigStructInit(&SSP_ConfigStruct);

	// Initialize SSP peripheral with parameter given in structure above
	SSP_Init(LPC_SSP1, &SSP_ConfigStruct);

	// Enable SSP peripheral
	SSP_Cmd(LPC_SSP1, ENABLE);

}

int main( void )
{
	 xQueue = xQueueCreate( 5, sizeof( int ) );





		if( xQueue != NULL )
		{

			xTaskCreate( vSenderTask, "Sender1", 240, ( void * ) 100, 1, NULL );


			xTaskCreate( vReceiverTask, "Receiver", 240, NULL, 2, NULL );

			vTaskStartScheduler();
		}
		else
		{

		}



	for( ;; );
	return 0;
}
static void intToString(int value, uint8_t* pBuf, uint32_t len, uint32_t base)
{
    static const char* pAscii = "0123456789abcdefghijklmnopqrstuvwxyz";
    int pos = 0;
    int tmpValue = value;

    // the buffer must not be null and at least have a length of 2 to handle one
    // digit and null-terminator
    if (pBuf == NULL || len < 2)
    {
        return;
    }

    // a valid base cannot be less than 2 or larger than 36
    // a base value of 2 means binary representation. A value of 1 would mean only zeros
    // a base larger than 36 can only be used if a larger alphabet were used.
    if (base < 2 || base > 36)
    {
        return;
    }

    // negative value
    if (value < 0)
    {
        tmpValue = -tmpValue;
        value    = -value;
        pBuf[pos++] = '-';
    }

    // calculate the required length of the buffer
    do {
        pos++;
        tmpValue /= base;
    } while(tmpValue > 0);


    if (pos > len)
    {
        // the len parameter is invalid.
        return;
    }

    pBuf[pos] = '\0';

    do {
        pBuf[--pos] = pAscii[value % base];
        value /= base;
    } while(value > 0);

    return;

}

/*-----------------------------------------------------------*/

static void vSenderTask( void *pvParameters )
{
long lValueToSend;
portBASE_TYPE xStatus;
int luz;
SensorLuz_new();
	 light_init();
	 light_enable();
	 light_setRange(LIGHT_RANGE_4000);
	lValueToSend = ( long ) pvParameters;

	for( ;; )
	{
		luz=SensorLuz_read();
		xStatus = xQueueSendToBack( xQueue, &luz, 0 );

		if( xStatus != pdPASS )
		{
			vPrintString( "Could not send to the queue.\r\n" );
		}

		taskYIELD();
	}
}

static void vReceiverTask( void *pvParameters )
{
	/* Declare the variable that will hold the values received from the queue. */
	long lReceivedValue;
	portBASE_TYPE xStatus;
	const portTickType xTicksToWait = 100 / portTICK_RATE_MS;
	init_ssp();
	oled_init();
	oled_clearScreen(OLED_COLOR_WHITE);
	oled_putString(1,1,  (uint8_t*)"Luz:", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
	for( ;; )
	{
		if( uxQueueMessagesWaiting( xQueue ) != 0 )
		{
			vPrintString( "Queue should have been empty!\r\n" );
		}
		xStatus = xQueueReceive( xQueue, &lReceivedValue, xTicksToWait );

		if( xStatus == pdPASS )
		{
			//vPrintStringAndNumber( "Received = ", lReceivedValue );
			intToString(lReceivedValue, buf, 10, 10);
			oled_fillRect((1 + 9 * 6), 1, 80, 8, OLED_COLOR_WHITE);
			oled_putString((1 + 9 * 6), 1, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
		}
		else
		{
			vPrintString( "Could not receive from the queue.\r\n" );
		}
	}
}

//void vContinuousProcessingTask( void *pvParameters )
//{
//char *pcTaskName;
//volatile unsigned long ul;
//
//	/* The string to print out is passed in via the parameter.  Cast this to a
//	character pointer. */
//	pcTaskName = ( char * ) pvParameters;
//
//	/* As per most tasks, this task is implemented in an infinite loop. */
//	for( ;; )
//	{
//		// Exibindo no display os valores
//						intToString(luz, buf, 10, 10);
//						oled_fillRect((1 + 9 * 6), 1, 80, 8, OLED_COLOR_WHITE);
//						oled_putString((1 + 9 * 6), 1, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);
//
//		/* Print out the name of this task.  This task just does this repeatedly
//		without ever blocking or delaying. */
//		vPrintString( pcTaskName );
//
//		/* A null loop has been inserted just to slow down the rate at which
//		messages are sent down the debug link to the console.  Without this
//		messages print out too quickly for the debugger display and controls
//		to keep up.  For clarity this null loop is not shown in the eBook text
//		as it is not relevant to the behaviour being demonstrated. */
//		for( ul = 0; ul < 0x1fff; ul++ )
//		{
//			asm volatile( "NOP" );
//		}
//	}
//}
///*-----------------------------------------------------------*/
//
//void vPeriodicTask( void *pvParameters )
//{
//portTickType xLastWakeTime;
//	luz=SensorLuz_read();
//	/* The xLastWakeTime variable needs to be initialized with the current tick
//	count.  Note that this is the only time we access this variable.  From this
//	point on xLastWakeTime is managed automatically by the vTaskDelayUntil()
//	API function. */
//	xLastWakeTime = xTaskGetTickCount();
//
//	/* As per most tasks, this task is implemented in an infinite loop. */
//	for( ;; )
//	{
//
//		/* Print out the name of this task. */
//		vPrintString( "Periodic task is running..........\n" );
//
//		/* We want this task to execute exactly every 10 milliseconds. */
//		vTaskDelayUntil( &xLastWakeTime, ( 10 / portTICK_RATE_MS ) );
//	}
//}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* This function will only be called if an API call to create a task, queue
	or semaphore fails because there is too little heap RAM remaining. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( xTaskHandle *pxTask, signed char *pcTaskName )
{
	/* This function will only be called if a task overflows its stack.  Note
	that stack overflow checking does slow down the context switch
	implementation. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
	/* This example does not use the idle hook to perform any processing. */
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
	/* This example does not use the tick hook to perform any processing. */
}


