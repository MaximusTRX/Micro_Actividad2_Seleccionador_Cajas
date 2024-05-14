/*
 * Seleccionador_Cajas.c
 *
 * Created: 5/8/2024 2:36:23 PM
 * Author : Maximus
 */ 

#include <avr/io.h>
// #include <iom328p.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/common.h>

//*** Nombres de pines ***
#define	ECHO		0
#define TRIGGER		1
#define	LEDBUILDIN	5


//*** Constantes propias ***
#define TIME1		50


// **** GPIOR0 como regsitros de banderas ****
#define F10MS		0	// GPIOR0<0>:
#define F100MS		1	// GPIOR0<1>:
#define F10US		2	// GPIOR0<2>:
//#define BTNACTUAL	3	// GPIOR0<3>:
#define ECHOFINISH	4	// GPIOR0<4>: 1 cuando la medición termina
#define ECHOSTATE	5	// GPIOR0<5>: 0 cuando se espera el flanco rising - 1 cuando se espera el falling
#define INIMED		6	// GPIOR0<6>:
#define FTRIGGER	7	// GPIOR0<7>:


// EEPROM
// eeprom_write_byte() para escribir en la eeprom
// eeprom_read_byte() para leer desde la eeprom

//EEMEM uint8_t variable1;

//.

// Constantes en FLASH

//PROGMEM const uint8_t varFlash = 10;

//.

//========================== Constantes Globales son guardadas en la SRAM

//uint8_t		tTRIGGER;		//!< Description: Tiempo entre inicio de medicion y medicion
uint8_t		time1;
//uint8_t		MASKHEARBEAT;
//uint16_t	tEchoUP;			//!< Description: Guarda el valor del contador del flanco ascendente del ECHO
//uint16_t	tEchoDOWN;			//!< Description: Guarda el valor del contador del flanco descendente del ECHO
//uint16_t	lastMedicion;		//!< Description: Guarda el valor de la resta entre los dos anteriores
uint8_t		buffTx[256];		//!< Description: Buffer para transmición de datos
uint8_t		indexWriteTx;		//!< Description: Indice de datos escrito en el buffer para enviar
uint8_t		indexReadTx;		//!< Description: Indice de datos leidos para ser enviados
//

typedef union{
	uint16_t	myword_16;
	uint8_t		myword_8[2];
}_uMyword;

_uMyword tEchoUP;
_uMyword tEchoDOWN;
_uMyword lastMedicion;

//========================== Declaración Cabeceras de Funciones
void ini_ports();
void ini_Timer1();
void ini_USART0();
void ini_ExtInterrupt();
void do_10ms();
//void do_Medicion();
//void do_BTNCHANGE();
void do_Transmit();
void do_Trigger();
void start_Med();

//.

//========================== Declaración Funciones de Interrupciones
ISR (TIMER1_COMPA_vect){
	//TCNT1	= 0x00;
    OCR1A	+= 20000;			// Equivale a 10ms
	GPIOR0	|= (1<<F10MS);
}

ISR (TIMER1_COMPB_vect){
	//TCNT1	= 0x00;
	OCR1B	+= 20;				// Equivale a 10us
	GPIOR0	|= (1<<F10US);
}

ISR(INT0_vect){
	if (!(GPIOR0 & (1<<ECHOSTATE)))
	{
		tEchoUP.myword_16 =	TCNT1;
		EICRA	=	0b00000011;		//!< Description: Configura el flanco a detectar, falling.
		GPIOR0	|=	(1<<ECHOSTATE);
	}else{
		tEchoDOWN.myword_16 = TCNT1;
		GPIOR0 |= (1<<ECHOFINISH);
	}
}



//========================== Initialization Functions
void ini_ports(){
	DDRB	= (1<<LEDBUILDIN) | (1<<TRIGGER);
	PORTB	= (1<<ECHO);
}

void ini_Timer1(){
	TCCR1A	= 0x00;
	TCCR1B	= 0b00000010;	// PRESSCALER /8 -> +1 cada 500ns
	OCR1A	= 20000;		// Equivale a 10ms
	OCR1B	= 20;			// Equivale a 10us
	TIMSK1	= 0b00000110;
	TIFR1	= TIFR1;
}

void ini_ExtInterrupt(){
	EICRA	= 0b00000011;	//!< Description: Habilita que flanco genera la interrupción. Rising: 1-1. Falling: 1-0.
	EIMSK	= (1<<INT0);				//!< Description: Habilita Interrupción general de los pines INT0
	EIFR	= EIFR;						//!< Description: Flag de interrupciones externas
}

void ini_USART0(){
	UCSR0A = UCSR0A;
	UCSR0A = (1<<U2X0);
	UCSR0B = (1<<TXEN0); //(1<<RXCIE0) | (1<<RXEN0) |
	UCSR0C = (1<< UCSZ01) | (1<< UCSZ00);
	UBRR0  = 0x10;
	//UBRR0H = (16>>8);
	//UBRR0L = 16;
}

//===================================== Functions
void do_10ms(){
	GPIOR0 &= ~(1<<F10MS);
	
	time1 --;
	if(!time1)
	{
		time1 = TIME1;
		GPIOR0 |= (1<<F100MS);
	}
}

void do_Transmit(){
	
}

void start_Med(){
	GPIOR0 |= (1<<FTRIGGER);
	GPIOR0 &= ~(1<<ECHOSTATE);
	GPIOR0 &= ~(1<<ECHOFINISH);
	
	EICRA	= (1<<ISC01) | (1<<ISC00);	//!< Description: Habilita que flanco genera la interrupción. Rising: 0-0. Falling: 1-0.
	//EIFR	= EIFR;						//!< Description: Flag de interrupciones externas
}

void do_Trigger(){
	
}

int main(void)
{
	// Ejemplo
	// GPIOR0 |= (1<<IS10MS) ES LO MISMO GPIOR0 |= _BV(IS10MS);
	// GPIOR0 |= (1<<5);  PONE SOLO El BIT 5 DEL REGISTRO EN 1
	// GPIOR0 &= ~(1<<5); PONE SOLO EL BIT 5 DEL REGISTRO EN 0
	
	// El START de assembler
	cli();
	ini_ports();
	ini_Timer1();
	ini_USART0();
	ini_ExtInterrupt();
	sei();
	
	indexReadTx = 0;
	indexWriteTx = 0;
	lastMedicion.myword_16 = 0;
	tEchoUP.myword_16 = 0;
	tEchoDOWN.myword_16 = 0;
	
	time1 = TIME1;
	
	
    while (1) 
    {
		
		if (GPIOR0 & (1<<F10US))
		{
			GPIOR0 &= ~(1<<F10US);
			
			if (GPIOR0 & (1<<FTRIGGER)){
				if (PINB & (1<<TRIGGER)){		//!< Description: Si el trigger está en 1 pongo la flag en 0
					GPIOR0 &= ~(1<<FTRIGGER);	//!< Description: Porque no necesito hacer toggle de nuevo
				}
				PORTB ^= (1<<TRIGGER);			//!< Description: Toggle del pin de trigger
			}
				
			
		}
		
		if (GPIOR0 & (1<<F10MS)){
			do_10ms();
		}
		
		if (GPIOR0 & (1<<ECHOFINISH))
		{
			GPIOR0 &= ~(1<<ECHOFINISH);
			lastMedicion.myword_16 = (tEchoDOWN.myword_16 - tEchoUP.myword_16)/58;
		}
		
		if(GPIOR0 & (1<<F100MS)){
			GPIOR0 &= ~(1<<F100MS);
			PORTB ^= (1<<LEDBUILDIN);
			buffTx[indexWriteTx++] = 'E';
			buffTx[indexWriteTx++] = 'U';
			buffTx[indexWriteTx++] = 'P';
			buffTx[indexWriteTx++] = ':'; 
			buffTx[indexWriteTx++] = tEchoUP.myword_8[0];
			buffTx[indexWriteTx++] = tEchoUP.myword_8[1];
			buffTx[indexWriteTx++] = ';';
			buffTx[indexWriteTx++] = 0x0A;
			
			buffTx[indexWriteTx++] = 'E';
			buffTx[indexWriteTx++] = 'D';
			buffTx[indexWriteTx++] = 'W';
			buffTx[indexWriteTx++] = ':';
			buffTx[indexWriteTx++] = tEchoDOWN.myword_8[0];
			buffTx[indexWriteTx++] = tEchoDOWN.myword_8[1];
			buffTx[indexWriteTx++] = ';';
			buffTx[indexWriteTx++] = 0x0A;
			
			buffTx[indexWriteTx++] = 'M';
			buffTx[indexWriteTx++] = 'E';
			buffTx[indexWriteTx++] = 'D';
			buffTx[indexWriteTx++] = ':';
			buffTx[indexWriteTx++] = lastMedicion.myword_8[0];
			buffTx[indexWriteTx++] = lastMedicion.myword_8[1];
			buffTx[indexWriteTx++] = ';';
			buffTx[indexWriteTx++] = 0x0A;
			
			start_Med();
		}
		
		if (UCSR0A & (1<<UDRE0))
			if (indexReadTx != indexWriteTx)
				UDR0 = buffTx[indexReadTx++];
		
		
    }
}

