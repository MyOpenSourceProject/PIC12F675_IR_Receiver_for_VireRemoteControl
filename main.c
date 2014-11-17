/* 
* 	NEC IR Remote Receiver using pic12F675
* 	File:   main.c
* 	Author:  Jithin Krishnan.K
* 	Rev. 0.0.1 : 03/03/2014 :  6:48 PM
* 
*	This program is free software: you can redistribute it and/or modify
*  	it under the terms of the GNU General Public License as published by
*  	the Free Software Foundation, either version 3 of the License, or
*	(at your option) any later version.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	You should have received a copy of the GNU General Public License
*	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*	opensourcecoderepo@gmail.com
*   The orginal code from Gaurav (mailchaduarygaurav@gmail.com) is modified 
*	to work with MpLab IDE v8.53 and Hi-tech c compiler 9.70
*   
************************************************************************/

#include <htc.h>

__CONFIG(UNPROTECT & BOREN & MCLRDIS & PWRTEN & WDTDIS & INTIO);

#define RELAY1		GPIO0
#define RELAY2		GPIO1	
#define RELAY3		GPIO2
#define RELAY4		GPIO4
#define LED_GREEN	GPIO5
#define IR_SENSOR 	GPIO3		/* IR PORT defination */ 
#define EEPROM_ENABLE			/* Enable EEPROM Configuration save */
//#define ENCODER					/* Change the module to an IR encoder */

#define TICKS11ms 	11044      	/* 11ms  */
#define TICKS5o5ms 	5522 		/* 5.5ms */
#define TICKS2o3ms 	2309 		/* 2.3ms */
#define TICKS3ms  	3012		/* 3ms   */
#define TICKS0o2ms	200			/* 0.2ms */
#define TICKS8ms 	8032

/* LT-03 Vire Commands */
#define POWER_OFF	0x48
#define	MODE		0x58
#define	MUTE		0x78
#define	PLAY_PAUSE	0x80
#define	RWD			0x40
#define	FWD			0xC0
#define	EQ			0x20
#define	VOL_MINUS	0xA0
#define VOL_PLUS	0x60
#define RPT			0x10
#define U_SD		0x90
#define	NUMERIC_0	0XE0
#define	NUMERIC_1	0x50
#define	NUMERIC_2	0xD8
#define NUMERIC_3	0xF8
#define	NUMERIC_4	0x30
#define	NUMERIC_5	0xB0
#define	NUMERIC_6	0x70
#define	NUMERIC_7	0x00
#define	NUMERIC_8	0xF0
#define	NUMERIC_9	0x98

unsigned int TIMEOUT	=	TICKS11ms;	/* The pulse should occur before this time excede Otherwise it is an error */ 
unsigned int PREPULSE	=	TICKS8ms;	/* The interrupt should occur after this time Otherwise it is an error		*/

static unsigned short long timer; 
static unsigned char DataReady;
static  unsigned char NEC_POS = 0;		/* (NEC_POS = NEC position) This varible is used to keep track of the edges of the input singal 
											as decoding of the singal is done by a state machine so this varible acutalley sotores what
											state we currently are and total bits 32 and 2 leading pulse */
 
static unsigned char address = 0, inv_address = 0;	/* These varible are used to store received address */
static unsigned char command = 0, inv_command = 0;	/* these varible are used to store received commands */
static unsigned char cmd = 0x13;

void interruptOnChangeIsr(void);  					/* interrupt service routine for interrupt on change of input port for IR sensor of the controller */ 
void timerInterruptIsr(void);						/* timer0 interrupt service rouine */ 

void interrupt t0intr(void)
{
	if(T0IF) {						/* Check the timer0 over flow interrupt flag */ 
		timerInterruptIsr();		/* timer0 overflow interrupt has been occur call the isr */
		T0IF =0;					/* clear the timer0 interrupt flag */ 
	} else if (GPIF) {				/* check the interrupt on change flag */
		interruptOnChangeIsr();		/* interrupt on change has been detected call the isr */	
		GPIF =0;					/* clear the interrupt on chage flag */
	}
}

/*
*  main function
*/
void main() 
{
   	ADCON0	= 0;		/* Shut off the A/D Converter */
	VRCON	= 0;		/* Shut off the Voltage Reference */
   
	CMCON	= 7;		/* disable the comparator */
    ANSEL	= 0;		/* all pin are Digital */
    TRISIO	= 8;   		/* Only GP3 is set to input rest are out */
	TMR0 	= 0;		/* clear the timer */
	OPTION	= 0x88;		/* pullups are disabled */
						/* timer0 clock source is internal */
						/* timer0 perscaller is 1:1 (disabled "assigned to WDT") */
	IOC = 0x8;   		/* interrupt on change is only to the GPIO3 */
	T0IE = 1;			/* Timer0 overflow interrupt enable */
	T0IF = 0;    		/* clear the timer0 intrrupt flags */
	GPIE = 1;			/* external interrupt on GPIO3 pin(4) is enabled */
	GPIF = 0;			/* clear the external interrrupt flag */
	PEIE = 1;    		/* peripheral intrrupt enable */
	GIE	 = 1;     		/* GLOBL interrupt enable */
	
#ifdef EEPROM_ENABLE  
		EEADR = 0x00;			/* load the state of port from EEPROM */ 
		RD = 1;					/* Start reding EEPORM */
		GPIO = EEDATA; 			/* LOAD The readed data form EEPORM to GPIO */
#endif
	
	while(1) {     				/* wait forever for  the data received and ready  */
		if (DataReady) {		/* data is received and ready to be procssed  	*/
			         
#ifndef ENCODER
			switch(command) {
			case NUMERIC_1: 
					RELAY1 = !RELAY1;		/* Toggle relay 1 */	
					break;
			case NUMERIC_2: 
					RELAY2 = !RELAY2;		/* Toggle relay 2 */
					break;
			case NUMERIC_3:
					RELAY3 = !RELAY3;		/* Toggle relay 3 */	
					break;
			case NUMERIC_4: 
					RELAY4 = !RELAY4;		/* Toggle relay 4 */
					break;
			case NUMERIC_5: 
					LED_GREEN = !LED_GREEN;	/* Toggle relay 4 */
				 	break;
			case POWER_OFF: 
					RELAY1 = 0;				/* Turn off all the relay */
				   	RELAY2 = 0;
				   	RELAY3 = 0;
				   	RELAY4 = 0;
					LED_GREEN = 0;
				    break;
			default :
					break;	
#else 																			/* 	Added this code to work this as an IR remote encoder */
			switch(command) {													/*	GPIO5:GPIO4:GPIO2:GPIO1:GPIO0 act as  5 Pin encoded out*/
			case NUMERIC_0:
					GPIO0 = 1;  GPIO1 = 0; GPIO2 = 1; GPIO4 = 0; GPIO5 = 1; /* 0x15 */
					break;
			case NUMERIC_1: 
					GPIO0 = 1;  GPIO1 = 0; GPIO2 = 0; GPIO4 = 0; GPIO5 = 0;	/* 0x01 */
					break;
			case NUMERIC_2: 
					GPIO0 = 0;  GPIO1 = 1; GPIO2 = 0; GPIO4 = 0; GPIO5 = 0;	/* 0x02 */
					break;
			case NUMERIC_3:
					GPIO0 = 1;  GPIO1 = 1; GPIO2 = 0; GPIO4 = 0; GPIO5 = 0;	/* 0x03 */
					break;
			case NUMERIC_4: 
					GPIO0 = 0;  GPIO1 = 0; GPIO2 = 1; GPIO4 = 0; GPIO5 = 0; /* 0x04 */
				   	break;
			case NUMERIC_5: 
					GPIO0 = 1;  GPIO1 = 0; GPIO2 = 1; GPIO4 = 0; GPIO5 = 0;	/* 0x05 */
				   	break;
			case NUMERIC_6: 
					GPIO0 = 0;  GPIO1 = 1; GPIO2 = 1; GPIO4 = 0; GPIO5 = 0;	/* 0x06 */
					break;
			case NUMERIC_7:
					GPIO0 = 1;  GPIO1 = 1; GPIO2 = 1; GPIO4 = 0; GPIO5 = 0;	/* 0x07 */
					break;
			case NUMERIC_8:
					GPIO0 = 0;  GPIO1 = 0; GPIO2 = 0; GPIO4 = 1; GPIO5 = 0; /* 0x08 */	
					break;
			case NUMERIC_9:
					GPIO0 = 1;  GPIO1 = 0; GPIO2 = 0; GPIO4 = 1; GPIO5 = 0; /* 0x09 */
					break;
			case POWER_OFF:
					GPIO0 = 0;  GPIO1 = 1; GPIO2 = 0; GPIO4 = 1; GPIO5 = 0;	/* 0x0A */
					break;
			case MODE: 
					GPIO0 = 1;  GPIO1 = 1; GPIO2 = 0; GPIO4 = 1; GPIO5 = 0;	/* 0x0B */
				   	break;
			case MUTE: 
					GPIO0 = 0;  GPIO1 = 0; GPIO2 = 1; GPIO4 = 1; GPIO5 = 0;	/* 0x0C */
				   	break;
			case PLAY_PAUSE: 
					GPIO0 = 1;  GPIO1 = 0; GPIO2 = 1; GPIO4 = 1; GPIO5 = 0;	/* 0x0D */
				    break;
			case RWD:
					GPIO0 = 0;  GPIO1 = 1; GPIO2 = 1; GPIO4 = 1; GPIO5 = 0;	/* 0x0E */
					break;
			case FWD: 
					GPIO0 = 1;  GPIO1 = 1; GPIO2 = 1; GPIO4 = 1; GPIO5 = 0;	/* 0x0F */
				   	break;
			case EQ:
					GPIO0 = 0;  GPIO1 = 0; GPIO2 = 0; GPIO4 = 0; GPIO5 = 1; /* 0x10 */
				   	break;
			case VOL_MINUS: 
					GPIO0 = 1;  GPIO1 = 0; GPIO2 = 0; GPIO4 = 0; GPIO5 = 1;	/* 0x11 */
				    break;
			case VOL_PLUS: 
					GPIO0 = 0;  GPIO1 = 1; GPIO2 = 0; GPIO4 = 0; GPIO5 = 1;	/* 0x12 */
				   	break;
			case RPT: 
					GPIO0 = 1;  GPIO1 = 1; GPIO2 = 0; GPIO4 = 0; GPIO5 = 1;	/* 0x13 */
				   	break;
			case U_SD: 
					GPIO0 = 0;  GPIO1 = 0; GPIO2 = 1; GPIO4 = 0; GPIO5 = 1;	/* 0x14 */
				    break;
			default :
					break;
#endif
		}

		
#ifdef EEPROM_ENABLE
		EEADR 	= 0x00; 					/* Write PORT status to EEPROM */
		EEDATA 	= GPIO;						/* load the current status of GPIO to EEPROM write register */
		WREN 	= 1;  						/* Enable EEPROM write */
		GIE 	= 0;						/* disable the interrupts as it may currupt the EEPROM data */
		EECON2 	= 0x55;		
		EECON2 	= 0xAA;		
		WR 		= 1;  						/* start writing */
 		GIE 	= 1;  						/* Enable the interrupts */ 
#endif	
		DataReady = 0;						/* data has been processed so clear the dataready flag */ 
	}
  }
}

/*
*  Port Change ISR
*/

void interruptOnChangeIsr(void)
{
	unsigned short long tdiff;       
	static unsigned long rxbuffer;
	unsigned char pin;

	tdiff = ((timer << 8)+ TMR0);	/* Calculate how much time has been passed since last interrupt */ 
									/* the time shold be less than time out and greater than PREPULSE */ 
	pin = IR_SENSOR;				/* Store the current status of Sensor */ 
	TMR0 = 0;						/* reset the timer0 to measure the next edge(interrupt) of input */
	timer = 0;						/* reset the timer varible to 0 */
	
if ((tdiff > PREPULSE) && (tdiff < TIMEOUT)) {		/* The edge (interrupt) occurrence time should be less than 
													   the TIMOUT and greater than PREPULESE else it is an fake singal 
													   At the very first edge (NEC_P)S == 0)  this conditon will always false
													   and the false block of this if will bring the state machine(Refer state Machie doc)
                                                       to position 1(position 1 means 9ms leading pulse has started now we have 
                                                       to wait for 4.5ms start pulse to occur)  */
						
				
	if ( NEC_POS == 1 || NEC_POS == 2) {			/* When we are here, it means 9ms leding pulse has ended and now we are NEC_POS = 1 
													   or NEC_POS = 2 */
			if ((pin == 1) && (NEC_POS == 1)) {
				NEC_POS++;
				TIMEOUT 	= TICKS5o5ms;  			/* timeout for 3rd pulse 5.5ms */	
				PREPULSE   	= TICKS3ms;				/* PREPULSE for 3rd pulse 3ms  */
			} else if (( pin == 0) && (NEC_POS == 2)) {
				NEC_POS++;
				TIMEOUT 	= TICKS2o3ms;  			/* now data starts so timeout is 2.3ms */
				PREPULSE   	= TICKS0o2ms;  
			} else { 						 		/* this block handle the conditon if any error occur after the completing the pre pulses */ 
				NEC_POS = 0;						/* reset the state machine */ 
				TIMEOUT 	=  	TICKS11ms;
				PREPULSE 	= 	TICKS8ms;
			}
		} else if (NEC_POS > 2) {					/* now we are picking the data */ 	
				NEC_POS++;					 		/* NEC_POS sill inrement on every edge */ 
			  	if (NEC_POS & 0x01) {				/* Here we check the if NEC_POS is an odd number because when NEC_POS goes greater than 3 then */ 
									    			/* NEC_POS will always be and odd value when a single bit tranmission is over */  
				    rxbuffer = rxbuffer << 1;		/* shift the buffer */
					if (tdiff > 1250) {				/* We are here means we just recevied the edge of finished tranmission of a bit */ 
													/* so if last edge was more than 1.24 ms then the bit which is just over is one else it is zero */ 
				 		rxbuffer = rxbuffer | 0x1; 
					} else {
						rxbuffer = rxbuffer |0x0;
			        }
			}
		if (NEC_POS > 66) {	 				      		/* we have reached (Leading pulse 2 + address 16 + ~address16 + command 16 + ~command 16 + last final 
												   		   burst first edge1) = 67th edge of the message frame means the date tranmission is now over  */	
				address 	= (rxbuffer>>24)& 0xFF;		/* extract the data from the buffer */ 
				inv_address = (rxbuffer>>16)& 0xFF;
				command 	= (rxbuffer>>8)	& 0xFF;
				inv_command = (rxbuffer)	& 0xFF;
				rxbuffer	= 0;

				if ((!(address & inv_address)) && (!(command & inv_command))) {		/* Check whether the received data is vaild or not */
						DataReady = 1;
						IR_SENSOR = !IR_SENSOR; /* Added for synchronization */
					} else 	{
						DataReady = 0;
				}
				TIMEOUT 	=  	TICKS11ms;					/* Whether we received the vaild data or not we have to reset the state machine */ 
				PREPULSE 	= 	TICKS8ms;
				NEC_POS = 0;
			}
	} else  {	
			TIMEOUT 	=	TICKS11ms;						/* some error occured reset state machine */ 
			PREPULSE 	=	TICKS8ms;
        }
	} else {
		if (pin == 0) {				/* we are here means that after a longtimeout or PREPULSE we just detect a pulse which may be the start of 9ms pulse */ 
		NEC_POS = 1;				 
	} else {	
	    NEC_POS = 0;				 
	}
	
	address		= 0xFF;
	inv_address = 0xFF;
	command 	= 0xFF;
	inv_command = 0xFF;
	DataReady 	= 0;
	TIMEOUT 	= TICKS11ms;		/* default timing */  
	PREPULSE 	= TICKS8ms;
   }
}

void timerInterruptIsr(void)
{
	if (timer < 0xFFFF);		/* this code is to increment the variable timer's value on every over flow */ 
								/* but this if conditon will prevent this variable form rollover when a long timeout occurs */
	 timer++;
}
