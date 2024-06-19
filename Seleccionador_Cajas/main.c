/*
 * Seleccionador_Cajas.c
 *
 * Created: 5/8/2024 2:36:23 PM
 * Author : Maxi y Santi
 */ 

#include <avr/io.h>
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
#define BOXSTACK	10	//cantidad para el buffer de cajas a tener en cuenta


typedef enum{
	ALIVE=0xF0,				//ALIVE QUE ENVIA EL MICRO AL SIMU
	ACK=0x0D,				//ACK QUE RECIVE EL MICRO DESDE EL SIMU
	CINTAON=0x50,			//ENVIA EL MICRO PARA ENCENDER LA CINTA
	CINTAOFF=0x51,			//RECIVE EL MICRO DATOS DE LA SIMULACI?N
	SACACAJA=0x52,			//ENVIA EL MICRO Y RECIVE, DATOS SOBRE LA CAJA A SACAR Y SI LO LOGR?
	CINTARESET=0x53,		//ENVIA EL MICRO PARA RESETEAR LA CINTA, RECIVE SI FUE REINICIADA O NO
	CINTAVEL=0x54,			//ENVIA EL MICRO VELOC O RECIVE UNA NUEVA VELOC
	BOXTYPE=0x5F,			//RECIVE LA ALTURA DE UNA NUEVA CAJA
}_eID;

typedef enum{
	START,
	HEADER_1,
	HEADER_2,
	HEADER_3,
	NBYTES,
	TOKEN,
	PAYLOAD
}_eProtocolo;

_eProtocolo estadoProtocolo;

typedef struct{
	uint8_t bufferRx[256];		//Buffer de recepcion de datos
	uint8_t bufferTx[256];		//Buffer de envio de datos
	uint8_t payload[32];		//!< Buffer para el Payload de datos recibidos
	uint8_t indexWriteRx;		//Indice de escritura del buffer circular de recepcion
	uint8_t indexReadRx;		//Indice de lectura del buffer circular de recepcion
	uint8_t indexWriteTx;		//Indice de escritura del buffer circular de transmision
	uint8_t indexReadTx;		//Indice de lectura del buffer circular de transmision
	
	uint8_t timeOut;         //!< TiemOut para reiniciar la máquina si se interrumpe la comunicación
	uint8_t cheksumRx;       //!< Cheksumm RX
	uint8_t cheksumtx;       //!< Cheksumm Tx
	
}_sDato;
_sDato datosProtocol;

typedef union{
	struct{
		uint8_t bit0: 1;
		uint8_t bit1: 1;
		uint8_t bit2: 1;
		uint8_t bit3: 1;
		uint8_t bit4: 1;
		uint8_t bit5: 1;
		uint8_t bit6: 1;
		uint8_t bit7: 1;
	}bits;
	uint8_t byte;
}_uFlags;

#define RXDATO				flags.bits.bit0
#define CINTASTATUS			flags.bits.bit1
#define GETTIME				flags.bits.bit2


 typedef struct{
	 uint8_t boxHeigt;
	 uint16_t enterTime;
	 uint16_t ejectTime;
	 uint8_t isAvailable; // se utiliza para ver si el array de estructura puede recibir una nueva caja
 }_snewBox;
 _snewBox myBox[BOXSTACK]; // estructura que asigna a cada caja una altura (salida), tiempo de entrada y tiempo de salida
 
 /*
// EEPROM
// eeprom_write_byte() para escribir en la eeprom
// eeprom_read_byte() para leer desde la eeprom
//EEMEM uint8_t variable1;
//.
// Constantes en FLASH
//PROGMEM const uint8_t varFlash = 10;

//.*/
//========================== Constantes Globales son guardadas en la SRAM

//uint8_t		tTRIGGER;		//!< Description: Tiempo entre inicio de medicion y medicion
uint8_t		time1;
uint8_t t100ms=0, t1s = 10,indexNewBox,indexCheckBox;			//contadores de tiempo
uint8_t speed,boxType[2],boxState,cintaState,newBox;
uint16_t cmOut[2],tSalida[2],tActual;
//uint8_t		MASKHEARBEAT;
//uint16_t		tEchoUP;			//!< Description: Guarda el valor del contador del flanco ascendente del ECHO
//uint16_t		tEchoDOWN;			//!< Description: Guarda el valor del contador del flanco descendente del ECHO
//uint16_t		lastMedicion;		//!< Description: Guarda el valor de la resta entre los dos anteriores


typedef union{
	int32_t i32;
	uint32_t ui32;
	uint16_t ui16[2];
	uint8_t ui8[4];
}_udat;
_udat myWord;

/*_uMyword tEchoUP;
_uMyword tEchoDOWN;
_uMyword lastMedicion;*/

//========================== Declaración Cabeceras de Funciones
void ini_ports();
void ini_Timer1();
void ini_USART0();
void ini_ExtInterrupt();
void do_10ms();
void do_Transmit();
//void do_Trigger();
//void start_Med();
void decodeProtocol();
void decodeData();
void doReset();
//.

//========================== Declaración Funciones de Interrupciones
ISR (TIMER1_COMPA_vect){
	//TCNT1	= 0x00;
    OCR1A	+= 20000;				// 20000 Equivale a 10ms
	GPIOR0	|= (1<<F10MS);			//bandera pasaron 10 ms
}

ISR (TIMER1_COMPB_vect){
	//TCNT1	= 0x00;			/*comento porque no uso jeje*/
	//OCR1B	+= 20;				// Equivale a 10us
	//GPIOR0	|= (1<<F10US);
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

ISR (USART_RX_vect){
	datosProtocol.bufferRx[datosProtocol.indexWriteRx++] = UDRE0;
	RXDATO=1;		//pongo en 1 la bandera de che leí un dato (chisme fresco)
}

//========================== Initialization Functions
void ini_ports(){
	DDRB	= (1<<LEDBUILDIN) | (1<<TRIGGER);
	PORTB	= (1<<ECHO);
}

void ini_Timer1(){
	TCCR1A	= 0x00;					//Timer en modo normal
	TCCR1B	= 0b00000010;			//PRESSCALER /8 -> +1 cada 500ns
	OCR1A	= 20000;				//Equivale a 10ms
	OCR1B	= 20;					//Equivale a 10us
	TIMSK1	= 0b00000110;			//Habilito la interrupción del compare A
	//TIFR1	= TIFR1;
	TIFR1 |= TIFR1;					//limpia las banderas de interrupcion
}

void ini_ExtInterrupt(){
	EICRA	= 0b00000011;	//!< Description: Habilita que flanco genera la interrupción. Rising: 1-1. Falling: 1-0.
	EIMSK	= (1<<INT0);				//!< Description: Habilita Interrupción general de los pines INT0
	EIFR	= EIFR;						//!< Description: Flag de interrupciones externas
}

void ini_USART0(){
	UCSR0A = UCSR0A;										//registro para saber si la transmición se hizo o si se puede trans
	UCSR0A = (1<<U2X0);										//duplico velocidad de transmision
	UCSR0B = (1 << RXCIE0) | (1 << RXEN0) | (1 << TXEN0);	//habilito interrupciones y datos de transmision/recepcion 
	UCSR0C = (1<< UCSZ01) | (1<< UCSZ00);					//seleccion de modo, cant bit a enviar, paridad
	UBRR0  = 0x10;											//Numero 16 configura el baudrate en 115200
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
	
	if(UCSR0A & _BV(UDRE0)){				//Me fijo si esta disponible para escribir
		UDR0=datosProtocol.bufferTx[datosProtocol.indexReadTx++];
	}
}

void start_Med(){
	GPIOR0 |= (1<<FTRIGGER);
	GPIOR0 &= ~(1<<ECHOSTATE);
	GPIOR0 &= ~(1<<ECHOFINISH);
	
	EICRA	= (1<<ISC01) | (1<<ISC00);	//!< Description: Habilita que flanco genera la interrupción. Rising: 0-0. Falling: 1-0.
	//EIFR	= EIFR;						//!< Description: Flag de interrupciones externas
}


void decodeProtocol(_sDato *datosCom)
{
	static uint8_t nBytes=0;// indice=0;
	while (datosCom->indexReadRx!=datosCom->indexWriteRx)
	{
		switch (estadoProtocolo) {
			case START:
			if (datosCom->bufferRx[datosCom->indexReadRx++]=='U'){
				estadoProtocolo=HEADER_1;
				datosCom->cheksumRx=0;
			}
			break;
			case HEADER_1:
			if (datosCom->bufferRx[datosCom->indexReadRx++]=='N')
			{
				estadoProtocolo=HEADER_2;
			}
			else{
				datosCom->indexReadRx--;
				estadoProtocolo=START;
			}
			break;
			case HEADER_2:
			if (datosCom->bufferRx[datosCom->indexReadRx++]=='E')
			{
				estadoProtocolo=HEADER_3;
			}
			else{
				datosCom->indexReadRx--;
				estadoProtocolo=START;
			}
			break;
			case HEADER_3:
			if (datosCom->bufferRx[datosCom->indexReadRx++]=='R')
			{
				estadoProtocolo=NBYTES;
			}
			else{
				datosCom->indexReadRx--;
				estadoProtocolo=START;
			}
			break;
			case NBYTES:
			//datosCom->indexStart=datosCom->indexReadRx; ///////////////////// AGREGAR ///////////////////////////////////
			nBytes=datosCom->bufferRx[datosCom->indexReadRx++];
			estadoProtocolo=TOKEN;
			break;
			case TOKEN:
			if (datosCom->bufferRx[datosCom->indexReadRx++]==':'){
				estadoProtocolo=PAYLOAD;
				datosCom->cheksumRx ='U'^'N'^'E'^'R'^ nBytes^':';
				// datosCom->payload[0]=nBytes;
				// indice=1;
			}
			else{
				datosCom->indexReadRx--;
				estadoProtocolo=START;
			}
			break;
			case PAYLOAD:
			if (nBytes>1){
				// datosCom->payload[indice++]=datosCom->bufferRx[datosCom->indexReadRx];
				datosCom->cheksumRx ^= datosCom->bufferRx[datosCom->indexReadRx++];
			}
			nBytes--;
			if(nBytes<=0){
				estadoProtocolo=START;
				if(datosCom->cheksumRx == datosCom->bufferRx[datosCom->indexReadRx]){
					decodeData(datosCom);
				}
			}
			break;
			default:
			estadoProtocolo=START;
			break;
		}
	}
}

void decodeData(){
		uint8_t auxBuffTx[50], indiceAux=0, cheksum;
		//auxBuffTx[indiceAux++]='U';
		//auxBuffTx[indiceAux++]='N';
		//auxBuffTx[indiceAux++]='E';
		//auxBuffTx[indiceAux++]='R';
		//auxBuffTx[indiceAux++]= 0 ;
		//auxBuffTx[indiceAux++]=':';
		
		switch (datosProtocol.indexReadRx){
			case ALIVE:
					auxBuffTx[indiceAux++]=ALIVE;
					auxBuffTx[indiceAux++]=ACK;
					//auxBuffTx[NBYTES]=0x03;
				break;
			case CINTAON:
					auxBuffTx[indiceAux++]=CINTAON;
					CINTASTATUS=1;
					speed=datosProtocol.payload[2];				//guarda la velocidad de la cinta
					boxType[0]=datosProtocol.payload[3];			// guarda el boxType0
					myWord.ui8[0]=datosProtocol.payload[4];
					myWord.ui8[1]=datosProtocol.payload[5];
					cmOut[0]=myWord.ui16[0];						// guarda el primer CM OUT
			
					boxType[1]=datosProtocol.payload[6];			// guarda el boxType1
					myWord.ui8[0]=datosProtocol.payload[7];
					myWord.ui8[1]=datosProtocol.payload[8];
					cmOut[1]=myWord.ui16[0];						// guarda el segundo CM OUT
			
					boxType[2]=datosProtocol.payload[9];			// guarda el boxType2
					myWord.ui8[0]=datosProtocol.payload[10];
					myWord.ui8[1]=datosProtocol.payload[11];
					cmOut[2]=myWord.ui16[0];						// guarda el ultimo CM OUT
					GETTIME=1;
					//auxBuffTx[NBYTES]=0x02;
				break;
			case CINTAOFF:
					auxBuffTx[indiceAux++] = CINTAOFF;
					CINTASTATUS = 0;
					//auxBuffTx[NBYTES] = 0x02;
				break;
			case SACACAJA:
					auxBuffTx[indiceAux++] = SACACAJA;
					auxBuffTx[indiceAux++] = myBox[indexCheckBox].boxHeigt; //envia la altura de la caja a tirar
					//auxBuffTx[NBYTES] = 0x03;
					myBox[indexCheckBox].isAvailable = 0;
				break;
			case CINTARESET:
					cintaState = datosProtocol.payload[2];
					if(cintaState == 0x0D)
					doReset();
				break;
			case CINTAVEL:
					auxBuffTx[indiceAux++] = 0x0D;
					speed = datosProtocol.payload[2];
					//auxBuffTx[NBYTES] = 0x02;
					GETTIME=1;								//flag para calcular el tiempo de salida
				break;
			case BOXTYPE:
					myBox[indexNewBox].boxHeigt = datosProtocol.payload[2];	//guarda el tipo de caja
					myBox[indexNewBox].enterTime = tActual;						//guarda el tiempo de entrada de la caja

					if(myBox[indexNewBox].boxHeigt == boxType[0])				//asigna el tiempo de salida según el tipo de caja
					myBox[indexNewBox].ejectTime = tActual+tSalida[0];
					if(myBox[indexNewBox].boxHeigt == boxType[1])
					myBox[indexNewBox].ejectTime = tActual+tSalida[1];
					if(myBox[indexNewBox].boxHeigt == boxType[2])
					myBox[indexNewBox].ejectTime = tActual+tSalida[2];
					myBox[indexNewBox].isAvailable = 1;							//pone en 1 la variable para no sobreescribir el array myBox
				break;
			default:
					auxBuffTx[indiceAux++] = 0xDD;
				break;
			cheksum = 0;
			for(uint8_t a=0 ;a < indiceAux ;a++){
				cheksum ^= auxBuffTx[a];
				datosProtocol.bufferTx[datosProtocol.indexWriteTx++] = auxBuffTx[a];
			}
			datosProtocol.bufferTx[datosProtocol.indexWriteTx++] = cheksum;
}
}

void doReset(){ //funcion de reseteo de la cinta
	//resetea el tiempo actual y borra los datos del array de las cajas.
	tActual = 0;
	for(int i=0;i<BOXSTACK;i++){
		myBox[i].boxHeigt = 0;
		myBox[i].ejectTime = 0;
		myBox[i].enterTime = 0;
	}
}

/*
void do_Trigger(){
	
}*/

int main(void){
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
	
	datosProtocol.indexReadTx = 0;
	datosProtocol.indexWriteTx = 0;
	datosProtocol.indexReadRx = 0;
	datosProtocol.indexWriteRx = 0;
	
	/*
	lastMedicion.myword_16 = 0;
	tEchoUP.myword_16 = 0;
	tEchoDOWN.myword_16 = 0;*/
	
	time1 = TIME1;
	
    while (1) 
    {
		if(RXDATO){
			decodeProtocol();
			RXDATO=0;
		}
		if(GETTIME){				
			for(int i=0;i<3;i++){
				tSalida[i]=(cmOut[i]/(speed*10));		//Paso la velocidad a CM
				tSalida[i]=(tSalida[i]*1000)/10;		//Paso a milisegundos
			}
			GETTIME=0;
		}
		if(datosProtocol.indexReadTx!=datosProtocol.indexWriteTx)
		do_Transmit();
		indexCheckBox++;
		if(indexCheckBox >=BOXSTACK )
		indexCheckBox=0;
		if(myBox[indexCheckBox].isAvailable){
			if(myBox[indexCheckBox].ejectTime < tActual){
				datosComProtocol.payload[1]=SACACAJA; //pone el ID en boxEJECT
				decodeData();
			}
			}else{ //si el available esta en 0 puedo cargar en ese indice
			indexNewBox=indexCheckBox;
		}
		
		
		
		
		
		
		
		
		/*
		if (GPIOR0 & (1<<F10US)){
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
			lastMedicion.myword_16 = (tEchoDOWN.myword_16 - tEchoUP.myword_16)/116;
		}
		
		if(GPIOR0 & (1<<F100MS)){
			GPIOR0 &= ~(1<<F100MS);
			PORTB ^= (1<<LEDBUILDIN);
			start_Med();
		}
		
		if (UCSR0A & (1<<UDRE0))
			if (datosProtocol.indexReadTx != datosProtocol.indexWriteTx)
				UDR0 = datosProtocol.bufferTx[datosProtocol.indexReadTx++];
    }
	*/
	
	
	
	
	
	
	
}

