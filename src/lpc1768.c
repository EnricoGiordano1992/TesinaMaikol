/*
===============================================================================
 Name        : lpc1768.c
 Author      : Enrico Giordano
 Version     : 1.0
 Copyright   : Copyright (C) 2013
 Description : funzioni elementari di lpc1768
===============================================================================
*/

#include "lpc1768.h"


void Delay(int delay)
{
	while(delay)
		delay--;
}


void resetGPIO(void)
{
	LPC_PINCON->PINSEL4 &= ~(0xFFFF); 	// Reset P2[0..7] = GPIO
	LPC_PINCON->PINSEL3 &= ~(3UL<<30); 	// Reset P1.31 = GPIO
	LPC_PINCON->PINSEL4 &= ~(0x03<<20);	// Reset P2.10 = GPIO
	LPC_PINCON->PINSEL3 &= ~(0x03<<18);
	LPC_PINCON->PINSEL3 &= ~(0x03<<20);
	LPC_PINCON->PINSEL3 &= ~(0x03<<24);
	LPC_PINCON->PINSEL3 &= ~(0x03<<26);
	LPC_PINCON->PINSEL7 &= ~(3 << 20);



}


void setOutput_LEDint(void)
{
	// Configurazione Pin GPIO = P2[0..7] Drive LED
		LPC_GPIO2->FIODIR |= 0xFF;			// P2[0..7] = Outputs
}

void setButton_reset(void)
{
	LPC_GPIO2->FIODIR &= ~(1UL<<10);	
}



void setOutput_ADC_Rint(void)
{
	// Configurazione convertitore A/D usando resistenza variabile integrata nel circuito
		LPC_PINCON->PINSEL3 |= (3UL << 30);		//configura pin1.31 per ricevere corrente
		LPC_SC->PCONP |= (1UL << 12); 			//abilita corrente sul convertitore AD
		LPC_ADC->ADCR = (1UL << 5)			//seleziona AD 0.5 per convertitore (dove c'è la resistenza variabile)
					  | (1UL << 8)		//conversione a 18 MHz / 2
					  | (1UL << 21);	//abilita ADC

}


void setBeep(void)
{
	LPC_PINCON->PINSEL7 &= ~(3 << 20);
	LPC_GPIO3->FIODIR |= (1UL<<26);
	LPC_GPIO3->FIOPIN ^= (1 << 26);

}

void stop_beep(void)
{
	if(!LPC_GPIO3->FIOPIN)
		LPC_GPIO3->FIOPIN ^= (1 << 26);
}

void setJoySwitch(void)
{
	LPC_GPIO1->FIODIR &= ~((1UL<<25)|(1UL<<26)|(1UL<<28)|(1UL<<29));	//P1.25,26,28,29=In

}

void turn_off_the_LEDS(void)
{
	LPC_GPIO2->FIOCLR =	0xFF;			// spegne tutti i LED
}


void turn_on_single_LED(int led)
{
	LPC_GPIO2->FIOSET = (1<<led);		// ON LED[led]
}


void turn_off_single_LED(int led)
{
	LPC_GPIO2->FIOCLR = (1<<led);		// OFF LED[led]
}


int reset_is_pressed(void)
{
	if ((LPC_GPIO2->FIOPIN >> 10) & 0x01)	//se viene rilasciato
		return 0;
	else					//se viene premuto
		return 1;

}


int joyswitch_up(void)
{
	if ((LPC_GPIO1->FIOPIN >> 25) & 0x01)
		return 0;
	else
		return 1;
}


int joyswitch_down(void)
{
	if ((LPC_GPIO1->FIOPIN >> 26) & 0x01)
		return 0;
	else
		return 1;
}

int joyswitch_right(void)
{
	if ((LPC_GPIO1->FIOPIN >> 28) & 0x01)
		return 0;
	else
		return 1;
}


int joyswitch_left(void)
{
	if ((LPC_GPIO1->FIOPIN >> 29) & 0x01)
		return 0;
	else
		return 1;
}

int joyswitch_pressed(void)
{
	if ((LPC_GPIO3->FIOPIN >> 25) & 0x01)
		return 0;
	else
		return 1;
}


void beep(int durata, int frequenza)
{
	while(durata)
	{
			LPC_GPIO3->FIOPIN ^= (1 << 26);
			Delay(frequenza);
			durata--;
	}
}


int convert_from_ADC_VR(void)
{
	int val;
	LPC_ADC->ADCR |= (1<<24);				// abilita la conversione
	while (!(LPC_ADC->ADGDR & (1UL<<31)));	//finchè non ha finito di convertire attendi
	val = ((LPC_ADC->ADGDR >> 4) & 0xFFF);	//ottieni il valore
	LPC_ADC->ADCR &= ~(7<<24);				//blocca la conversione fino alla prossima chiamata di funzione

	return val;
}
