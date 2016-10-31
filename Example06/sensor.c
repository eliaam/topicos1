#include "lpc17xx_pinsel.h"
#include "lpc17xx_i2c.h"
#include "lpc17xx_gpio.h"
#include "lpc17xx_ssp.h"
#include "lpc17xx_adc.h"
#include "lpc17xx_timer.h"
#include "oled.h"
#include "acc.h"
#include "rgb.h"
#include "light.h"
#include "sensor.h"
static void Sensor_init_i2c(void);
static void change7Seg();
void Timer_init(uint32_t time);

typedef struct tag_sensor
{
	int luz_data;
}ttag_sensor;

ttag_sensor ClassHandle;

void SensorLuz_new(void)
{
	Sensor_init_i2c();
}

int SensorLuz_read(void)
{
	ClassHandle.luz_data=light_read();
	return ClassHandle.luz_data;
}

static void Sensor_init_i2c(void)
{
	PINSEL_CFG_Type PinCfg;

	/* Initialize I2C2 pin connect */
	PinCfg.Funcnum = 2;
	PinCfg.Pinnum = 10;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);
	PinCfg.Pinnum = 11;
	PINSEL_ConfigPin(&PinCfg);

	// Initialize I2C peripheral
	I2C_Init(LPC_I2C2, 100000);

	/* Enable I2C1 operation */
	I2C_Cmd(LPC_I2C2, ENABLE);
}

void Timer_init(uint32_t time)
{
	TIM_TIMERCFG_Type TIM_ConfigStruct;
	TIM_MATCHCFG_Type TIM_MatchConfigStruct ;

// Initialize timer 0, prescale count time of 1ms
	TIM_ConfigStruct.PrescaleOption = TIM_PRESCALE_USVAL;
	TIM_ConfigStruct.PrescaleValue	= 1000;
	// use channel 0, MR0
	TIM_MatchConfigStruct.MatchChannel = 0;
	// Enable interrupt when MR0 matches the value in TC register
	TIM_MatchConfigStruct.IntOnMatch   = TRUE;
	//Enable reset on MR0: TIMER will not reset if MR0 matches it
	TIM_MatchConfigStruct.ResetOnMatch = FALSE;
	//Stop on MR0 if MR0 matches it
	TIM_MatchConfigStruct.StopOnMatch  = TRUE;
	//do no thing for external output
	TIM_MatchConfigStruct.ExtMatchOutputType =TIM_EXTMATCH_NOTHING;
	// Set Match value, count value is time (timer * 1000uS =timer mS )
	TIM_MatchConfigStruct.MatchValue   = time;

	// Set configuration for Tim_config and Tim_MatchConfig
	TIM_Init(LPC_TIM0, TIM_TIMER_MODE,&TIM_ConfigStruct);
	TIM_ConfigMatch(LPC_TIM0,&TIM_MatchConfigStruct);
	// To start timer 0
	TIM_Cmd(LPC_TIM0,ENABLE);
	while ( !(TIM_GetIntStatus(LPC_TIM0,0)));
		TIM_ClearIntPending(LPC_TIM0,0);
	NVIC_EnableIRQ(TIMER0_IRQn);
}


static uint8_t ch7seg = '9';

void TIMER0_IRQHandler(){


	NVIC_DisableIRQ(TIMER0_IRQn);



	        if(ClassHandle.luz_data==0)
	        {
	        	change7Seg();
	        }
	        else
	        {
	        	ch7seg='9';
	        	led7seg_setChar(ch7seg, FALSE);
	        }
}

static void change7Seg()
{

	 if(ch7seg == '0' && light_read()==0){
		  led7seg_setChar(ch7seg, FALSE);
		  rgb_setLeds(RGB_GREEN);
		  Timer0_Wait(1000);
	 }else{
	 led7seg_setChar(ch7seg, FALSE);
	 ch7seg--;
	 Timer0_Wait(1000);
	 }
}


