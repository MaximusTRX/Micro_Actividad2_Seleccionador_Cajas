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
#define TIME1		30


// **** GPIOR0 como regsitros de banderas ****
#define F10MS		0	// GPIOR0<0>:
#define F100MS		1	// GPIOR0<1>:
#define F10US		2	// GPIOR0<2>:
//#define BTNACTUAL	3	// GPIOR0<3>:
//#define BTNDOWN		4	// GPIOR0<4>:
//#define ECHOFLAG	5	// GPIOR0<5>:
//#define INIMED		6	// GPIOR0<6>:
//#define FTRIGGER	7	// GPIOR0<7>:


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
//uint8_t		tdebounce;
uint8_t		t100ms;
//uint8_t		MASKHEARBEAT;
//uint16_t	tEchoUP;			//!< Description: Guarda el valor del contador del flanco ascendente del ECHO
//uint16_t	tEchoDOWN;			//!< Description: Guarda el valor del contador del flanco descendente del ECHO
uint8_t		lastMedicion;		//!< Description: Guarda el valor de la resta entre los dos anteriores
uint8_t		buffTx[256];		//!< Description: Buffer para transmición de datos
uint8_t		indexWriteTx;		//!< Description: Indice de datos escrito en el buffer para enviar
uint8_t		indexReadTx;		//!< Description: Indice de datos leidos para ser enviados
//


//========================== Declaración Cabeceras de Funciones
void ini_ports();
void ini_Timer1();
void do_10ms();
//void do_Medicion();
//void do_BTNCHANGE();
void do_Transmit();

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
	
	t100ms --;
	if(!t100ms)
	{
		t100ms = TIME1;
		GPIOR0 |= (1<<F100MS);
	}
}


void do_Transmit(){
	if (UCSR0A & (1<<UDRE0)){
		UDR0 = lastMedicion;					//!< Description: UDR0 es el registro que se carga para mandar los datos
	}
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
	sei();
	
	//lastMedicion = 0x44;
	indexReadTx = 0;
	indexWriteTx = 0;
	
	t100ms = TIME1;
	lastMedicion = 0;
	
    while (1) 
    {
		
		if (GPIOR0 & (1<<F10US))
		{
			GPIOR0 &= ~(1<<F10US);
		}
		
		if (GPIOR0 & (1<<F10MS)){
			do_10ms();
		}
		
		if(GPIOR0 & (1<<F100MS)){
			GPIOR0 &= ~(1<<F100MS);
			PORTB ^= (1<<LEDBUILDIN);
			buffTx[indexWriteTx++] = 'M';
			buffTx[indexWriteTx++] = 'E';
			buffTx[indexWriteTx++] = '1';
			buffTx[indexWriteTx++] = ':';
			buffTx[indexWriteTx++] = lastMedicion;
			buffTx[indexWriteTx++] = ';';
			buffTx[indexWriteTx++] = 0x0A;
			lastMedicion++;
			
		}
		
		if (UCSR0A & (1<<UDRE0)){
			if (indexReadTx != indexWriteTx)
			{
				UDR0 = buffTx[indexReadTx++];
			}
		}
		
		
    }
}

