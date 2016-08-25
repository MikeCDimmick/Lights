#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>


/* This line specifies the frequency your AVR is running at.
   This code supports 20 MHz, 16 MHz and 8MHz */

// These lines specify what pin the LED strip is on.
// You will either need to attach the LED strip's data line to PB0 or change these
// lines to specify a different pin.
//data line is PB0
#define LED_STRIP_PORT PORTB
#define LED_STRIP_DDR  DDRB
#define LED_STRIP_PIN  0

//defines the baudrate and calculates the requires register value to set it
#define USART_BAUDRATE 9600
#define UBRR_VALUE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)


//number of LEDs in strip, a strip of x length, connected to a strip of y length can be controlled like a strip of (x+y) length
#define LED_COUNT 300

//defines the number of levels the ADC can take on
#define ADC_LEVELS 255

//defines the voltage value picked up on PC0
volatile double voltage;

//state of the program, changed accordingly when a communication byte is recognized
volatile int state = 0;

//***************************************************************************************************************************************************STRUCTURES

/*  The rgb_color struct represents the color for an 8-bit RGB LED.

    Examples:
	
      Black:      (rgb_color){ 0, 0, 0 }
      Pure red:   (rgb_color){ 255, 0, 0 }
      Pure green: (rgb_color){ 0, 255, 0 }
      Pure blue:  (rgb_color){ 0, 0, 255 }
      White:      (rgb_color){ 255, 255, 255}
 */

typedef struct rgb_color
{
  unsigned char red, green, blue;
} rgb_color;

//declare rgb array
rgb_color colors[LED_COUNT];

//******************************************************************************************************************************************Function Prototypes

void __attribute__((noinline)) led_strip_write(rgb_color * colors, unsigned int count);
void USART0Init(void);
int recieveThreeDigitNumber();
void ADCInit();
void ShiftPattern(rgb_color* colorArray);
void updateState(int newState);
void connectionSuccess();
void RecieveColour();
void Off();
void On();
void rainbowPattern();
void smartLights();


//**************************************************************************************************************************************************LED WRITING


/** led_strip_write sends a series of colors to the LED strip, updating the LEDs.
 The colors parameter should point to an array of rgb_color structs that hold the colors to send.
 The count parameter is the number of colors to send.

 This function takes about 1.1 ms to update 30 LEDs.
 Interrupts must be disabled during that time, so any interrupt-based library
 can be negatively affected by this function.
 
 Timing details at 20 MHz (the numbers slightly different at 16 MHz and 8MHz):
  0 pulse  = 400 ns
  1 pulse  = 850 ns
  "period" = 1300 ns
 */
void __attribute__((noinline)) led_strip_write(rgb_color * colors, unsigned int count) 
{
  // Set the pin to be an output driving low.
  LED_STRIP_PORT &= ~(1<<LED_STRIP_PIN);
  LED_STRIP_DDR |= (1<<LED_STRIP_PIN);

  cli();   // Disable interrupts temporarily because we don't want our pulse timing to be messed up.
  while(count--)
  {
    // Send a color to the LED strip.
    // The assembly below also increments the 'colors' pointer,
    // it will be pointing to the next color at the end of this loop.
    asm volatile(
        "ld __tmp_reg__, %a0+\n"
        "ld __tmp_reg__, %a0\n"
        "rcall send_led_strip_byte%=\n"  // Send red component.
        "ld __tmp_reg__, -%a0\n"
        "rcall send_led_strip_byte%=\n"  // Send green component.
        "ld __tmp_reg__, %a0+\n"
        "ld __tmp_reg__, %a0+\n"
        "ld __tmp_reg__, %a0+\n"
        "rcall send_led_strip_byte%=\n"  // Send blue component.
        "rjmp led_strip_asm_end%=\n"     // Jump past the assembly subroutines.

        // send_led_strip_byte subroutine:  Sends a byte to the LED strip.
        "send_led_strip_byte%=:\n"
        "rcall send_led_strip_bit%=\n"  // Send most-significant bit (bit 7).
        "rcall send_led_strip_bit%=\n"
        "rcall send_led_strip_bit%=\n"
        "rcall send_led_strip_bit%=\n"
        "rcall send_led_strip_bit%=\n"
        "rcall send_led_strip_bit%=\n"
        "rcall send_led_strip_bit%=\n"
        "rcall send_led_strip_bit%=\n"  // Send least-significant bit (bit 0).
        "ret\n"

        // send_led_strip_bit subroutine:  Sends single bit to the LED strip by driving the data line
        // high for some time.  The amount of time the line is high depends on whether the bit is 0 or 1,
        // but this function always takes the same time (2 us).
        "send_led_strip_bit%=:\n"
#if F_CPU == 8000000
        "rol __tmp_reg__\n"                      // Rotate left through carry.
#endif
        "sbi %2, %3\n"                           // Drive the line high.

#if F_CPU != 8000000
        "rol __tmp_reg__\n"                      // Rotate left through carry.
#endif

#if F_CPU == 16000000
        "nop\n" "nop\n"
#elif F_CPU == 20000000
        "nop\n" "nop\n" "nop\n" "nop\n"
#elif F_CPU != 8000000
#error "Unsupported F_CPU"
#endif

        "brcs .+2\n" "cbi %2, %3\n"              // If the bit to send is 0, drive the line low now.

#if F_CPU == 8000000
        "nop\n" "nop\n"
#elif F_CPU == 16000000
        "nop\n" "nop\n" "nop\n" "nop\n" "nop\n"
#elif F_CPU == 20000000
        "nop\n" "nop\n" "nop\n" "nop\n" "nop\n"
        "nop\n" "nop\n"
#endif

        "brcc .+2\n" "cbi %2, %3\n"              // If the bit to send is 1, drive the line low now.

        "ret\n"
        "led_strip_asm_end%=: "
        : "=b" (colors)
        : "0" (colors),         // %a0 points to the next color to display
          "I" (_SFR_IO_ADDR(LED_STRIP_PORT)),   // %2 is the port register (e.g. PORTC)
          "I" (LED_STRIP_PIN)     // %3 is the pin number (0-8)
    );

    // Uncomment the line below to temporarily enable interrupts between each color.
    //sei(); asm volatile("nop\n"); cli();
  }
  sei();          // Re-enable interrupts now that we are done.
  _delay_us(50);  // Hold the line low for 15 microseconds to send the reset signal.
}

//********************************************************************************************************************************************************USART

void USART0Init(void)
{
	// Set baud rate
	UBRR0H = (uint8_t)(UBRR_VALUE>>8);
	UBRR0L = (uint8_t)UBRR_VALUE;
	// Set frame format to 8 data bits, no parity, 1 stop bit
	UCSR0C |= (1<<UCSZ01)|(1<<UCSZ00);
	//enable reception and RC complete interrupt
	UCSR0B |= (1<<RXEN0)|(1<<RXCIE0);
}

//this is used to pick up the R, G, or B value from the bluetooth module. 0 is 000, 15 is 015, and 255 is 255. there are always 3 digits and the python program ensures that
int recieveThreeDigitNumber()
{
	char recieved_byte[4];

	// wait until a bytes are ready to read
	while( ( UCSR0A & ( 1 << RXC0 ) ) == 0 ){}
	
	// grab the bytes from the serial port, with a slight delay inbetween each
	recieved_byte[0] = UDR0;
	_delay_ms(5);
	recieved_byte[1] = UDR0;
	_delay_ms(5);
	recieved_byte[2] = UDR0;
	recieved_byte[3] = '\0';
	
	//turn individual numbers in the array into a full int, and return that int
	int val = atoi(recieved_byte);
	return val;
}

//**********************************************************************************************************************************************************ADC

//setup continuous sampling with interrupts on completion on PA0
void ADCInit()
{
	cli();//disable interrupts

	//set up continuous sampling of analog pin 0

	//clear ADCSRA and ADCSRB registers
	ADCSRA = 0;
	ADCSRB = 0;

	ADMUX |= (1 << REFS0);									 //set reference voltage
	ADMUX |= (1 << ADLAR);									 //left align the ADC value- so we can read highest 8 bits from ADCH register only
	ADCSRA |= (1 << ADPS2) | (1 << ADPS0) | (1 << ADPS1);	 //set ADC clock with 128 prescaler- 16mHz/32=500kHz
	ADCSRA |= (1 << ADATE);									 //enable auto trigger
	ADCSRA |= (1 << ADEN);									 //enable ADC
	ADCSRA |= (1 << ADSC);									 //start ADC measurements
	ADCSRA |= (1 << ADIE);									 //enable interrupts when measurement complete

	sei();//enable interrupts
}

//************************************************************************************************************************************************AUX FUNCTIONS


//Used to shift a static pattern across the strip
void ShiftPattern(rgb_color* colorArray)
{
	//moving backwards through the light strip...
	for(int i = LED_COUNT; i >= 0; i--)
	{
		//if this is the first LED, set it equal to whatever the last LED is
		if(i == 0)
			colorArray[0] = colorArray[LED_COUNT-1];
		
		//if its not the first LED then set it to whatever the LED infront of it is
		else
			colorArray[i]=colorArray[i-1];
	}
	//this delay determines how fast the pattern moves across the strip
	_delay_ms(20);
	
	//write the data out after the entire line has been shifted
	led_strip_write(colorArray, LED_COUNT);
}


//**********************************************************************************************************************************************State Functions

/*
			State:												Communication Byte:													Associated Function in Micro					Associated Function in Python
			________________________________________________________________________________________________________________________________________________________________________________________________________
			0: State of nothingness								NONE																NONE											NONE
			
			1: Connection Success								A: Sent when computer and micro are successfully connected			connectionSuccess()								connectBluetoothDevice()
			
			2: Recieved a Colour								B: Sent when user sends a colour									RecieveColour()									getColor(), randomColour()

			3: Turn lights off									C: Sent when the user wants the lights to turn off					off()											Off()

			4: Turn lights on white (50,50,50)					D: Sent when user wants the lights on								On()											On()
			
			5: Lights are changed based on photosensor			E: Sent when user actives smartlights								smartLights()									smartLights()

			6: Dynamic rainbow pattern on the strip				F: sent by user to activate pattern									rainbowPattern()								rainbow Pattern()

			6: Dynamic rainbow pattern on the strip				F: sent by user to activate pattern									rainbowPattern()								rainbow Pattern()

			6: Dynamic rainbow pattern on the strip				F: sent by user to activate pattern									rainbowPattern()								rainbow Pattern()

			6: Dynamic rainbow pattern on the strip				F: sent by user to activate pattern									rainbowPattern()								rainbow Pattern()

			6: Dynamic rainbow pattern on the strip				F: sent by user to activate pattern									rainbowPattern()								rainbow Pattern()

			6: Dynamic rainbow pattern on the strip				F: sent by user to activate pattern									rainbowPattern()								rainbow Pattern()

			6: Dynamic rainbow pattern on the strip				F: sent by user to activate pattern									rainbowPattern()								rainbow Pattern()

			6: Dynamic rainbow pattern on the strip				F: sent by user to activate pattern									rainbowPattern()								rainbow Pattern()

			6: Dynamic rainbow pattern on the strip				F: sent by user to activate pattern									rainbowPattern()								rainbow Pattern()

			6: Dynamic rainbow pattern on the strip				F: sent by user to activate pattern									rainbowPattern()								rainbow Pattern()

			6: Dynamic rainbow pattern on the strip				F: sent by user to activate pattern									rainbowPattern()								rainbow Pattern()

			6: Dynamic rainbow pattern on the strip				F: sent by user to activate pattern									rainbowPattern()								rainbow Pattern()


*/

//sets the new state
void updateState(int newState)
{
	state = newState;
}

//flashes first LED green three times to signify a success connection, --- state: 1, communication byte: 'A'
void connectionSuccess()
{
	//set entire strip to 0,0,0
	unsigned int i;
	for(i = 0; i < LED_COUNT; i++)
	{
		colors[i] = (rgb_color){ 0,0,0 };
	}
	
	//flash just the first LED 3 times
	for(i=0; i < 3; i++)
	{
		colors[0] = (rgb_color){ 0,255,0 };
		led_strip_write(colors, LED_COUNT);
		_delay_ms(200);
		colors[0] = (rgb_color){ 0,0,0 };
		led_strip_write(colors, LED_COUNT);
		_delay_ms(200);
	}
	
	//update the state to 3 which turns on the LEDs on a low brightness setting
	updateState(3);
}

//read in an RGB colour and upload it to the entire strip, --- state: 2, communication byte: 'B'
void RecieveColour()
{	
	cli();
	int R = recieveThreeDigitNumber();
	int G = recieveThreeDigitNumber();
	int B = recieveThreeDigitNumber();
	unsigned int i;
	for(i = 0; i < LED_COUNT; i++)
	{
		colors[i] = (rgb_color){R,G,B};
	}
	
	led_strip_write(colors, LED_COUNT);
	updateState(0);
	sei();
}

//turn off all the lights, --- state: 3, communication byte: 'C'
void Off()
{	
	unsigned int i;
	for(i = 0; i < LED_COUNT; i++)
		colors[i] = (rgb_color){0,0,0};
	
	led_strip_write(colors, LED_COUNT);
	updateState(0);
}

//turn on all the lights, --- state: 4, communication byte: 'D'
void On()
{
	unsigned int i;
	for(i = 0; i < LED_COUNT; i++)
		colors[i] = (rgb_color){50, 50, 50};
	
	led_strip_write(colors, LED_COUNT);
	updateState(0);
}

//dynamic rainbow pattern shifts across strip
void rainbowPattern()
{
	//violet, indigo, blue, green, yellow, orange, red
	rgb_color rainbow[7] = {
							{148,0,211},
							{75,0,130},
							{0,0,255},
							{0,255,0},
							{255,255,0},
							{255,127,0},
							{255,0,0}
							};
							
	int counter = 0;
	for(int i = 0; i < LED_COUNT; i++)
	{
		colors[i] = rainbow[counter];
		counter++;
		if(counter == 7)
			counter = 0;
	}	
	
	while(state == 6)
		ShiftPattern(colors);

}

//uses ADC to sample a voltage divider between a 10k resistor and a photo resistor I had lying around
void smartLights()
{
	while(state == 5)
	{	
		//average 25 samples over 25ms so the dimming/bright feature doesnt seem as spikey
		int sum =0;
		int lightLevel;
		for(int i =0; i<25;i++ )
		{
			sum += (voltage/5.0)*255;
			_delay_ms(1);
		}
		lightLevel = (sum/25);
		
		//arbitrary scale factor, needs some work...
		lightLevel += 50*lightLevel/255;
		
		//make sure the light level never falls out of range, could also use some work
		if(lightLevel < 0)
			lightLevel = 0;
			
		//write the color out to the strip
		unsigned int i;
		for(i = 0; i < LED_COUNT; i++)
		{
			colors[i] = (rgb_color){lightLevel,lightLevel,lightLevel};
		}
		
		led_strip_write(colors, LED_COUNT);
	}
}


//*********************************************************************************************************************************************************main

int main (void)
{
	//Initialize USART0 and ADC
	USART0Init();
	ADCInit();
	
	//enable global interrupts
	sei();
	while(1)
	{			
		//re enable interrupts on USART receive 
		UCSR0B |= (1<<RXEN0)|(1<<RXCIE0);
			
		//based on the state of the program, decide which function to proceed with	
		switch (state)
		{
			//connect to blue tooth
			case 1:
			connectionSuccess();
			break;
				
			//set to a solid colour
			case 2:
			RecieveColour();
			break;
				
			//turn on the lights
			case 3:
			On();
			break;
				
			//turn off the lights
			case 4:
			Off();
			break;
				
			//let the lights decide how bright to be based on ambient light in the room
			case 5:
			smartLights();
			break;
				
			case 6:
			rainbowPattern();
			break;
		}
	}
}

//**********************************************************************************************************************************************************ISR

// Interrupt service routine for the ADC completion, read the voltage and put it in the variable "voltage" which is a volatile int
ISR(ADC_vect)
{
	cli();		
	voltage = ADCH*5/255.0;
	sei();		
}


//RX Complete interrupt service routine
ISR(USART_RX_vect)
{
	cli();
	
	//read in the byte
	uint8_t temp;
	temp = UDR0;
	
	//each case is handled based on its respective Communication Byte (for more information, see the states table commented under the state functions header)
	switch (temp)
	{
		//connect to bluetooth
		case 'A':
		updateState(1);
		break;
				 
		//solid colour
		case 'B':
		updateState(2);
		break;
				 
		//turn off
		case 'C':
		updateState(3);
		break;
				 
		//turn on
		case 'D':
		updateState(4);
		break;
				 
		//Smart lights
		case 'E':
		updateState(5);
		break;
		
		case 'F':
		updateState(6);
		break;
	}
	sei();
}

//To be added**************************************************************************************************************************************************

	//static unsigned int time = 0;
	//while(state == 6)
	//{
	//for(int i = 0; i < LED_COUNT; i++)
	//{
	//unsigned char x = (time >> 2) - 8*i;
	//colors[i] = (rgb_color){ 255 - x, x, 255-x };
	//}
	//
	//led_strip_write(colors, LED_COUNT);
	//_delay_ms(20);
	//time += 20;
	//}
