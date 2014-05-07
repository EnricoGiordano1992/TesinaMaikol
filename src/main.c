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

int led_flag = 0;	//all'inizio è spento

int logout_flag = 0;

void gestisci_richiesta();


/* ----------------------- Start implementation -----------------------------*/
int
main( void )
{
	//initialize lpc1768

	SystemInit();

	resetGPIO();
	setOutput_LEDint();
	turn_off_the_LEDS();

	memcpy(WebSide,WebSide_senza_password,sizeof(WebSide_senza_password));

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





int LED_action()
{
	int i = 0;

	//search '?' character
	while(TCP_RX_BUF[i] != '/')
		i++;

	//search "led" string
	if(TCP_RX_BUF[++i] == 'l' && TCP_RX_BUF[++i] == 'e' && TCP_RX_BUF[++i] == 'd')
		return 1;

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

   	/*
    	 * se non ho ricevuto ancora la password o l'ho ricevuta sbagliata,
    	 * il flag "pw_flag" è = 0
    	 */
    	if(pw_flag == 0)
    		//pw control
    		if(PW_search() == 1)
    		{
    			pw_flag = 1;
    			memcpy(WebSide,WebSide_con_password,sizeof(WebSide_con_password));
    			logout_flag = 1;
    		}

    	/*
    	 * se ho ricevuto la richiesta led
    	 *
    	 */
    	if(LED_action() == 1)
    	{
    		if(led_flag == 0)
    			//accendo il led
    			turn_on_single_LED(0);
    		else
    			turn_off_single_LED(0);

    		led_flag = !led_flag;

    	}

    	if(logout() == 1 && logout_flag == 1)
    	{
			memcpy(WebSide,WebSide_senza_password,sizeof(WebSide_senza_password));
			led_flag = 0;
			pw_flag = 0;

			logout_flag = 0;
    	}

}
