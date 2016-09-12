/*****************************************************************************
 *   Peripherals such as temp sensor, light sensor, accelerometer,
 *   and trim potentiometer are monitored and values are written to
 *   the OLED display.
 *
 *   Copyright(C) 2010, Embedded Artists AB
 *   All rights reserved.
 *
 ******************************************************************************/



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

#define NOTE_PIN_HIGH() GPIO_SetValue(0, 1<<26);
#define NOTE_PIN_LOW()  GPIO_ClearValue(0, 1<<26);

static uint8_t buf[10];

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

static void init_i2c(void)
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

static void init_adc(void)
{
	PINSEL_CFG_Type PinCfg;

	/*
	 * Init ADC pin connect
	 * AD0.0 on P0.23
	 */
	PinCfg.Funcnum = 1;
	PinCfg.OpenDrain = 0;
	PinCfg.Pinmode = 0;
	PinCfg.Pinnum = 23;
	PinCfg.Portnum = 0;
	PINSEL_ConfigPin(&PinCfg);

	/* Configuration for ADC :
	 * 	Frequency at 1Mhz
	 *  ADC channel 0, no Interrupt
	 */
	ADC_Init(LPC_ADC, 1000000);
	ADC_IntConfig(LPC_ADC,ADC_CHANNEL_0,DISABLE);
	ADC_ChannelCmd(LPC_ADC,ADC_CHANNEL_0,ENABLE);

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
int32_t xoff;
   int32_t yoff;
   int32_t zoff;
   uint32_t luz;

   int8_t x;
   int8_t y;
   int8_t z;
int main (void)
{

    init_i2c();
    init_ssp();
    init_adc();
    light_init();

    oled_init();
    acc_init();
    rgb_init();
    led7seg_init();




    oled_clearScreen(OLED_COLOR_WHITE);

    oled_putString(1,1,  (uint8_t*)"Luz:", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
    oled_putString(1,25, (uint8_t*)"Acc x  : ", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
    oled_putString(1,33, (uint8_t*)"Acc y  : ", OLED_COLOR_BLACK, OLED_COLOR_WHITE);
    oled_putString(1,41, (uint8_t*)"Acc z  : ", OLED_COLOR_BLACK, OLED_COLOR_WHITE);

    rgb_setLeds(RGB_BLUE|RGB_GREEN|RGB_RED);
    light_enable();
    light_setRange(LIGHT_RANGE_4000);
    btn1 = ((GPIO_ReadValue(0) >> 4) & 0x01);



    while(1) {
    	Timer_init(500);
    	// Exibindo no display os valores
    		                intToString(luz, buf, 10, 10);
    		                oled_fillRect((1 + 9 * 6), 1, 80, 8, OLED_COLOR_WHITE);
    		                oled_putString((1 + 9 * 6), 1, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

    		                intToString(x, buf, 10, 10);
    		                oled_fillRect((1+9*6),25, 80, 32, OLED_COLOR_WHITE);
    		                oled_putString((1+9*6),25, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

    		                intToString(y, buf, 10, 10);
    		                oled_fillRect((1+9*6),33, 80, 40, OLED_COLOR_WHITE);
    		                oled_putString((1+9*6),33, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

    		                intToString(z, buf, 10, 10);
    		                oled_fillRect((1+9*6),41, 80, 48, OLED_COLOR_WHITE);
    		                oled_putString((1+9*6),41, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

    }

}

void TIMER0_IRQHandler(){


	    acc_read(&x, &y, &z);
	    xoff = 0-x;
	    yoff = 0-y;
	    zoff = 64-z;

	NVIC_DisableIRQ(TIMER0_IRQn);
	 // Leitura do aceleromêtro
	 acc_read(&x, &y, &z);
	 x = x+xoff;
	        y = y+yoff;
	        z = z+zoff;

	        // Leitura da luz
	        luz = light_read();

	        // Exibindo no display os valores
	        intToString(luz, buf, 10, 10);
	        oled_fillRect((1 + 9 * 6), 1, 80, 8, OLED_COLOR_WHITE);
	        oled_putString((1 + 9 * 6), 1, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

	        intToString(x, buf, 10, 10);
	        oled_fillRect((1+9*6),25, 80, 32, OLED_COLOR_WHITE);
	        oled_putString((1+9*6),25, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

	        intToString(y, buf, 10, 10);
	        oled_fillRect((1+9*6),33, 80, 40, OLED_COLOR_WHITE);
	        oled_putString((1+9*6),33, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);

	        intToString(z, buf, 10, 10);
	        oled_fillRect((1+9*6),41, 80, 48, OLED_COLOR_WHITE);
	        oled_putString((1+9*6),41, buf, OLED_COLOR_BLACK, OLED_COLOR_WHITE);


	        if(luz==0)
	        {
	        	change7Seg();
	        }
	        else
	        {
	        	ch7seg='9';
	        	led7seg_setChar(ch7seg, FALSE);
	        }


}





