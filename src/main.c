/*
===============================================================================
 Name        : main.c
 Author      : Enrico Giordano
 Version     :
 Copyright   : Copyright (C) 
 Description : main definition
===============================================================================
 */

#include "lpc1768.h"

#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include <cr_section_macros.h>
#include <NXP/crp.h>

#define extern

#include "easyweb.h"

#include "ethmac.h"								// MAC level (low level)

#include "tcpip.h"                               // easyWEB TCP/IP stack

#include "webside.h"                             // website for our HTTP server (HTML)


#define PW			"Gianni93"
#define PW_LENGTH	sizeof(PW)

int pw_flag = 0;	//no password

int led_flag[7] = {0};


int logout_flag = 0;

void gestisci_richiesta();


#define LED_NUM     8                   /* Number of user LEDs                */
const unsigned long led_mask[] = { 1UL<<0, 1UL<<1, 1UL<<2, 1UL<< 3,
		1UL<< 4, 1UL<< 5, 1UL<< 6, 1UL<< 7 };


/*----------------------------------------------------------------------------
  Function that turns on requested LED
 *----------------------------------------------------------------------------*/
void LED_On (unsigned int num) {

	LPC_GPIO2->FIOPIN |= led_mask[num];
}

/*----------------------------------------------------------------------------
  Function that turns off requested LED
 *----------------------------------------------------------------------------*/
void LED_Off (unsigned int num) {

	LPC_GPIO2->FIOPIN &= ~led_mask[num];
}


volatile uint16_t GusSinTable[45] =                                       /* ÕýÏÒ±í                       */
{
		410, 467, 523, 576, 627, 673, 714, 749, 778,
		799, 813, 819, 817, 807, 789, 764, 732, 694,
		650, 602, 550, 495, 438, 381, 324, 270, 217,
		169, 125, 87 , 55 , 30 , 12 , 2  , 0  , 6  ,
		20 , 41 , 70 , 105, 146, 193, 243, 297, 353
};



/* ----------------------- Start implementation -----------------------------*/
int
main( void )
{
	int i;
	int j;

	//initialize lpc1768

	SystemInit();

	LPC_GPIO2->FIODIR |= 0x000000ff;  //P2.0...P2.7 Output LEDs on PORT2 defined as Output

	LPC_PINCON->PINSEL1 = 0x00200000;	/* set p0.26 to DAC output */


	for(i = 0; i < 100; i++)
		for(j = 0; j < 100; j++)
			WebSide[i][j] = '\0';

	for(i = 0; i < 7; i++)
		led_flag[i] = 0;

	for(i = 0; i < 7; i++)
		LED_Off(i);

	memcpy(WebSide,WebSide_senza_password,sizeof(char) * sizeof(WebSide_senza_password));

	TCPLowLevelInit();								//stack tcp init (low level, so it's a hardware init)

	HTTPStatus = 0;                                // clear HTTP-server's flag register

	TCPLocalPort = TCP_PORT_HTTP;                  // set port we want to listen to

	while(1)
	{
		if (!(SocketStatus & SOCK_ACTIVE)) TCPPassiveOpen();   // listen for incoming TCP-connection
		DoNetworkStuff();                                      // handle network and easyWEB-stack
		// events
		HTTPServer();

	}
}


// This function implements a very simple dynamic HTTP-server.
// It waits until connected, then sends a HTTP-header and the
// HTML-code stored in memory. Before sending, it replaces
// some special strings with dynamic values.
// NOTE: For strings crossing page boundaries, replacing will
// not work. In this case, simply add some extra lines
// (e.g. CR and LFs) to the HTML-code.

void HTTPServer(void)
{
	if (SocketStatus & SOCK_CONNECTED)             // check if somebody has connected to our TCP
	{
		if (SocketStatus & SOCK_DATA_AVAILABLE)      // check if remote TCP sent data
		{

			gestisci_richiesta();

			TCPReleaseRxBuffer();                      // and throw it away
		}

		if (SocketStatus & SOCK_TX_BUF_RELEASED)     // check if buffer is free for TX
		{
			if (!(HTTPStatus & HTTP_SEND_PAGE))        // init byte-counter and pointer to webside
			{                                          // if called the 1st time
				HTTPBytesToSend = sizeof(WebSide) - 1;   // get HTML length, ignore trailing zero
				PWebSide = (unsigned char *)WebSide;     // pointer to HTML-code
			}

			if (HTTPBytesToSend > MAX_TCP_TX_DATA_SIZE)     // transmit a segment of MAX_SIZE
			{
				if (!(HTTPStatus & HTTP_SEND_PAGE))           // 1st time, include HTTP-header
				{
					//copy response http message 200 (OK) into tcp tx buffer (tx = transmit)
					memcpy(TCP_TX_BUF, GetResponse, sizeof(GetResponse) - 1);
					//copy response (Pwebside = pointer (to) website) into tcp tx buffer (tx = transmit)
					memcpy(TCP_TX_BUF + sizeof(GetResponse) - 1, PWebSide, MAX_TCP_TX_DATA_SIZE - sizeof(GetResponse) + 1);
					//number of byte to send
					HTTPBytesToSend -= MAX_TCP_TX_DATA_SIZE - sizeof(GetResponse) + 1;

					PWebSide += MAX_TCP_TX_DATA_SIZE - sizeof(GetResponse) + 1;
				}
				else
				{
					memcpy(TCP_TX_BUF, PWebSide, MAX_TCP_TX_DATA_SIZE);
					HTTPBytesToSend -= MAX_TCP_TX_DATA_SIZE;
					PWebSide += MAX_TCP_TX_DATA_SIZE;
				}

				TCPTxDataCount = MAX_TCP_TX_DATA_SIZE;   // bytes to transfer
				TCPTransmitTxBuffer();                   // transfer buffer
			}
			else if (HTTPBytesToSend)                  // transmit leftover bytes
			{
				memcpy(TCP_TX_BUF, PWebSide, HTTPBytesToSend);
				TCPTxDataCount = HTTPBytesToSend;        // bytes to transfer
				TCPTransmitTxBuffer();                   // send last segment
				TCPClose();                              // and close connection
				HTTPBytesToSend = 0;                     // all data sent
			}

			HTTPStatus |= HTTP_SEND_PAGE;              // ok, 1st loop executed
		}
	}
	else
		HTTPStatus &= ~HTTP_SEND_PAGE;               // reset help-flag if not connected
}



int PW_search()
{
	int i = 0;
	int j = 0;

	char pw_buffer[PW_LENGTH -1];
	char pw[] = PW;

	//search '?' character
	while(TCP_RX_BUF[i] != '?')
		i++;

	//search "pw=" string
	if(TCP_RX_BUF[++i] == 'p' && TCP_RX_BUF[++i] == 'w' && TCP_RX_BUF[++i] == '=')
	{
		++i;
		//pw control
		/*
		 * copio all'interno del buffer pw_buffer tutto ciò che si trova dopo i (pw=) del buffer TCP_RX_BUF
		 * fino alla lunghezza della nostra password (PW_LENGHT-1)
		 *
		 */
		strncpy(pw_buffer, (char *)&TCP_RX_BUF[i], PW_LENGTH-1);


		/*
		 * confronto carattere per carattere la password ricevuta con quella prestabilita
		 *
		 */
		for(j = 0;j < PW_LENGTH -1; j++)
			if(pw_buffer[j] != pw[j])
				return 0;

		return 1;
	}

	return 0;
}



int SOUND_action()
{
	int i = 0;

	//search '?' character
	while(TCP_RX_BUF[i] != '/')
		i++;

	//search "led" string
	if(TCP_RX_BUF[++i] == 's' && TCP_RX_BUF[++i] == 'o' && TCP_RX_BUF[++i] == 'u'  && TCP_RX_BUF[++i] == 'n'  && TCP_RX_BUF[++i] == 'd')
		return 1;
	return 0;
}


int LED_action()
{
	int i = 0;

	//search '?' character
	while(TCP_RX_BUF[i] != '/')
		i++;

	//search "led" string
	if(TCP_RX_BUF[++i] == 'l' && TCP_RX_BUF[++i] == 'e' && TCP_RX_BUF[++i] == 'd')
		if(TCP_RX_BUF[++i] == '?')
			if(TCP_RX_BUF[++i] == 'l' && TCP_RX_BUF[++i] == 'e' && TCP_RX_BUF[++i] == 'd' && TCP_RX_BUF[++i] == '=' )
				return TCP_RX_BUF[++i] - 48;

	return 0;
}


int logout()
{
	int i = 0;

	//search '?' character
	while(TCP_RX_BUF[i] != '/')
		i++;

	//search "log" string
	if(TCP_RX_BUF[++i] == 'l' && TCP_RX_BUF[++i] == 'o' && TCP_RX_BUF[++i] == 'g')
		return 1;

	return 0;


}



void gestisci_richiesta()
{

	int i;
	int j;

	int req;

	/*
	 * se non ho ricevuto ancora la password o l'ho ricevuta sbagliata,
	 * il flag "pw_flag" è = 0
	 */
	if(pw_flag == 0)
		//pw control
		if(PW_search() == 1)
		{
			pw_flag = 1;

			for(i = 0; i < 100; i++)
				for(j = 0; j < 100; j++)
					WebSide[i][j] = '\0';

			memcpy(WebSide,WebSide_con_password,sizeof(char) * sizeof(WebSide_con_password));
			logout_flag = 1;
		}

	/*
	 * se ho ricevuto la richiesta led
	 *
	 */
	req = LED_action();
	if(req != 0)
	{
		req--;

		if(led_flag[req])
			LED_On(req);
		else
			LED_Off(req);

		led_flag[req] = !led_flag[req];
	}

	if(logout() == 1 && logout_flag == 1)
	{
		for(i = 0; i < 100; i++)
			for(j = 0; j < 100; j++)
				WebSide[i][j] = '\0';

		memcpy(WebSide,WebSide_senza_password,sizeof(char) * sizeof(WebSide_senza_password));

		for(i = 0; i < 7; i++)
			led_flag[i] = 0;

		pw_flag = 0;

		logout_flag = 0;
	}

	if(SOUND_action())
	{
		int j = 100;
		while(j--){
			for(i = 0; i < 1000; i++)
				LPC_DAC->DACR = 255;

			for(i = 0; i < 1000; i++)
				LPC_DAC->DACR = -255;


			for (i = 0; i < 45; i++)
			{
				LPC_DAC->DACR = (GusSinTable[i] << 6);                               /* Êä³öÕýÏÒ²š                   */
			}
		}
	}

}
