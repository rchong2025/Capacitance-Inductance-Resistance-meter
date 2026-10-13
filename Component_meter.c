#include <XC.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lcd.h"
 
// Configuration Bits (somehow XC32 takes care of this)
#pragma config FNOSC = FRCPLL       // Internal Fast RC oscillator (8 MHz) w/ PLL
#pragma config FPLLIDIV = DIV_2     // Divide FRC before PLL (now 4 MHz)
#pragma config FPLLMUL = MUL_20     // PLL Multiply (now 80 MHz)
#pragma config FPLLODIV = DIV_2     // Divide After PLL (now 40 MHz)
#pragma config FWDTEN = OFF         // Watchdog Timer Disabled
#pragma config FPBDIV = DIV_1       // PBCLK = SYCLK
#pragma config FSOSCEN = OFF        // Secondary Oscillator Enable (Disabled)

// Defines
#define SYSCLK 40000000L
#define Baud2BRG(desired_baud)( (SYSCLK / (16*desired_baud))-1)

#define SAVE_BUTTON   (PORTB&(1<<15))
#define PRINT_BUTTON  (PORTB&(1<<14))
#define SERIAL_SWITCH (PORTB&(1<<13))
#define UNIT_SWITCH   (PORTB&(1<<12))
#define IND_SWITCH    (PORTB&(1<<10))
#define RES_SWITCH	  (PORTB&(1<<1))

#define ADC_MAX_VAL      1023.0f   // 1023 for 10-bit ADC (change to 4095.0f if using 12-bit ADC)
#define V_REF            3.3f      // PIC32 ADC Reference Voltage
#define REF_TEMP_C       25.0f     // Baseline temperature for capacitance
#define VOLTAGE_DIV_MULT 2.0f      // Multiplier to reverse the 10k/10k voltage divider
 
void UART2Configure(int baud_rate)
{
    // Peripheral Pin Select
    U2RXRbits.U2RXR = 4;    //SET RX to RB8
    RPB9Rbits.RPB9R = 2;    //SET RB9 to TX

    U2MODE = 0;         // disable autobaud, TX and RX enabled only, 8N1, idle=HIGH
    U2STA = 0x1400;     // enable TX and RX
    U2BRG = Baud2BRG(baud_rate); // U2BRG = (FPb / (16*baud)) - 1
    
    U2MODESET = 0x8000;     // enable UART2
}

// Needed to by scanf() and gets()
int _mon_getc(int canblock)
{
	char c;
	
    if (canblock)
    {
	    while( !U2STAbits.URXDA); // wait (block) until data available in RX buffer
	    c=U2RXREG;
	    if(c=='\r') c='\n'; // When using PUTTY, pressing <Enter> sends '\r'.  Ctrl-J sends '\n'
		return (int)c;
    }
    else
    {
        if (U2STAbits.URXDA) // if data available in RX buffer
        {
		    c=U2RXREG;
		    if(c=='\r') c='\n';
			return (int)c;
        }
        else
        {
            return -1; // no characters to return
        }
    }
}

// Use the core timer to wait for 1 ms.
void wait_1ms(void)
{
    unsigned int ui;
    _CP0_SET_COUNT(0); // resets the core timer count

    // get the core timer count
    while ( _CP0_GET_COUNT() < (SYSCLK/(2*1000)) );
}

void Timer4us(unsigned char t) 
{
	T4CON = 0x8000; // enable Timer4, source PBCLK, 1:1 prescaler
 
    // delay 100us per loop until less than 100us remain
    while( t >= 100){
        t-=100;
        TMR4=0;
        while(TMR4 < SYSCLK/10000L);
    }
 
    // delay 10us per loop until less than 10us remain
    while( t >= 10){
        t-=10;
        TMR4=0;
        while(TMR4 < SYSCLK/100000L);
    }
 
    // delay 1us per loop until finished
    while( t > 0)
    {
        t--;
        TMR4=0;
        while(TMR4 < SYSCLK/1000000L);
    }
    // turn off Timer4 so function is self-contained
    T4CONCLR=0x8000;
}

void waitms(unsigned int ms)
{
	unsigned int j;
	unsigned char k;
	for(j=0;j<ms;j++)
		for(k=0;k<4;k++)
			Timer4us(250);
}

void LCD_pulse(void)
{
	LCD_E = 1;
	Timer4us(40);
	LCD_E = 0;
}

void LCD_byte(unsigned char x)
{
	LCD_D7=(x&0x80)?1:0;
	LCD_D6=(x&0x40)?1:0;
	LCD_D5=(x&0x20)?1:0;
	LCD_D4=(x&0x10)?1:0;
	LCD_pulse();
	Timer4us(40);
	LCD_D7=(x&0x08)?1:0;
	LCD_D6=(x&0x04)?1:0;
	LCD_D5=(x&0x02)?1:0;
	LCD_D4=(x&0x01)?1:0;
	LCD_pulse();
}

void WriteData(unsigned char x)
{
	LCD_RS = 1;
	LCD_byte(x);
	waitms(2);
}

void WriteCommand(unsigned char x)
{
	LCD_RS = 0;
	LCD_byte(x);
	waitms(5);
}

void LCD_4BIT(void)
{
	// Configure the pins used to communicate with the LCD as outputs
	LCD_RS_ENABLE = 0;
	LCD_E_ENABLE = 0;
	LCD_D4_ENABLE = 0;
	LCD_D5_ENABLE = 0;
	LCD_D6_ENABLE = 0;
	LCD_D7_ENABLE = 0;
	
	LCD_E = 0; // Resting state of LCD's enable is zero
	// LCD_RW = 0; Not used in this code.  Connect to ground.
	waitms(20);
	// First make sure the LCD is in 8-bit mdode, then change to 4-bit mode
	WriteCommand(0x33);
	WriteCommand(0x33);
	WriteCommand(0x32); // Change to 4-bit mode
	
	// Configure the LCD
	WriteCommand(0x28);
	WriteCommand(0x0c);
	WriteCommand(0x01); // Clear screen command (takes some time)
	waitms(20); // Wait for clear screen command to finish
	LATBbits.LATB0 = 	!LATBbits.LATB0;
}

void LCDprint(char * string, unsigned char line, unsigned char clear)
{
	int j;
	
	WriteCommand(line==2?0xc0:0x80);
	waitms(5);
	for(j=0;string[j]!=0;j++)
		WriteData(string[j]); //Write the message character by character
	if(clear)
		for(;j<CHARS_PER_LINE;j++)
			WriteData(' '); //Clear the rest of the line if clear is 1
}

#define PIN_PERIOD  (PORTB&(1<<6))
#define PIN_PERIOD2 (PORTB&(1<<0))

// GetPeriod() seems to work fine for frequencies between 200Hz and 700kHz.

long int GetPeriod (int n, int pin)
{
	int i;
	unsigned int saved_TCNT1a, saved_TCNT1b;
	
    _CP0_SET_COUNT(0); // resets the core timer count
    if (pin == 0)
		while (PIN_PERIOD!=0) // Wait for square wave to be 0
		{
			if(_CP0_GET_COUNT() > (SYSCLK/4)) return 0;
		}
	else 
		while (PIN_PERIOD2!=0) // Wait for square wave to be 0
		{
			if(_CP0_GET_COUNT() > (SYSCLK/4)) return -1;
		}

    _CP0_SET_COUNT(0); // resets the core timer count
    if (pin == 0)
		while (PIN_PERIOD==0) // Wait for square wave to be 1
		{
			if(_CP0_GET_COUNT() > (SYSCLK/4)) return 0;
		}
	else 
		while (PIN_PERIOD2==0) // Wait for square wave to be 1
		{
			if(_CP0_GET_COUNT() > (SYSCLK/4)) return -2;
		}
	
    _CP0_SET_COUNT(0); // resets the core timer count
	for(i=0; i<n; i++) // Measure the time of 'n' periods
	{	
		if (pin == 0) {
			while (PIN_PERIOD!=0) // Wait for square wave to be 0
			{
				if(_CP0_GET_COUNT() > (SYSCLK/4)) return 0;
			}
			while (PIN_PERIOD==0) // Wait for square wave to be 1
			{
				if(_CP0_GET_COUNT() > (SYSCLK/4)) return 0;
			}
		} else {
			while (PIN_PERIOD2!=0) // Wait for square wave to be 0
			{
				if(_CP0_GET_COUNT() > (SYSCLK/4)) return 0;
			}
			while (PIN_PERIOD2==0) // Wait for square wave to be 1
			{
				if(_CP0_GET_COUNT() > (SYSCLK/4)) return 0;
			}
		}
	}

	return  _CP0_GET_COUNT();
}

int SerialTransmit(const char *buffer)
{
    unsigned int size = strlen(buffer);
    while( size)
    {
        while( U2STAbits.UTXBF);    // wait while TX buffer full
        U2TXREG = *buffer;          // send single character to transmit buffer
        buffer++;                   // transmit next character on following loop
        size--;                     // loop until all characters sent (when size = 0)
    }
 
    while( !U2STAbits.TRMT);        // wait for last transmission to finish
 
    return 0;
}

void ADCConf(void)
{
    AD1CON1CLR = 0x8000;    // disable ADC before configuration
    AD1CON1 = 0x00E0;       // internal counter ends sampling and starts conversion (auto-convert), manual sample
    AD1CON2 = 0;            // AD1CON2<15:13> set voltage reference to pins AVSS/AVDD
    AD1CON3 = 0x0f01;       // TAD = 4*TPB, acquisition time = 15*TAD 
    AD1CON1SET=0x8000;      // Enable ADC
}

int ADCRead(char analogPIN)
{
    AD1CHS = analogPIN << 16;    // AD1CHS<16:19> controls which analog pin goes to the ADC
 
    AD1CON1bits.SAMP = 1;        // Begin sampling
    while(AD1CON1bits.SAMP);     // wait until acquisition is done
    while(!AD1CON1bits.DONE);    // wait until conversion done
 
    return ADC1BUF0;             // result stored in ADC1BUF0
}

float get_temperature_celsius(uint32_t adc_raw_value) {
    // 1. Calculate voltage at the PIC32 Pin
    float v_pin = ((float)adc_raw_value / ADC_MAX_VAL) * V_REF;
    
    // 2. Reverse the voltage divider to find actual LM335 voltage
    float v_lm335 = v_pin * VOLTAGE_DIV_MULT;
    
    // 3. Convert to Kelvin (10mV per degree K)
    float temp_k = v_lm335 / 0.01f;
    
    // 4. Convert to Celsius
    return temp_k - 273.15f;
}

/**
 * Plugs into your existing code. 
 * Pass your calculated capacitance and the current temperature into this.
 */
float apply_temp_compensation(float raw_capacitance, float current_temp_c) {

	float TEMP_COEFF = 0.00018f; 
    float delta_t = current_temp_c - REF_TEMP_C;
    
    // Apply linear compensation formula
    float compensated_cap = raw_capacitance / (1.0f + (TEMP_COEFF * delta_t));
    
    return compensated_cap;
}

/* Pinout for DIP28 PIC32MX130:
                                          --------
                                   MCLR -|1     28|- AVDD 
  VREF+/CVREF+/AN0/C3INC/RPA0/CTED1/RA0 -|2     27|- AVSS 
        VREF-/CVREF-/AN1/RPA1/CTED2/RA1 -|3     26|- AN9/C3INA/RPB15/SCK2/CTED6/PMCS1/RB15
   PGED1/AN2/C1IND/C2INB/C3IND/RPB0/RB0 -|4     25|- CVREFOUT/AN10/C3INB/RPB14/SCK1/CTED5/PMWR/RB14
  PGEC1/AN3/C1INC/C2INA/RPB1/CTED12/RB1 -|5     24|- AN11/RPB13/CTPLS/PMRD/RB13
   AN4/C1INB/C2IND/RPB2/SDA2/CTED13/RB2 -|6     23|- AN12/PMD0/RB12
     AN5/C1INA/C2INC/RTCC/RPB3/SCL2/RB3 -|7     22|- PGEC2/TMS/RPB11/PMD1/RB11
                                    VSS -|8     21|- PGED2/RPB10/CTED11/PMD2/RB10
                     OSC1/CLKI/RPA2/RA2 -|9     20|- VCAP
                OSC2/CLKO/RPA3/PMA0/RA3 -|10    19|- VSS
                         SOSCI/RPB4/RB4 -|11    18|- TDO/RPB9/SDA1/CTED4/PMD3/RB9
         SOSCO/RPA4/T1CK/CTED9/PMA1/RA4 -|12    17|- TCK/RPB8/SCL1/CTED10/PMD4/RB8
                                    VDD -|13    16|- TDI/RPB7/CTED3/PMD5/INT0/RB7
                    PGED3/RPB5/PMD7/RB5 -|14    15|- PGEC3/RPB6/PMD6/RB6
                                          --------
*/

// Information here:
// http://umassamherstm5.org/tech-tutorials/pic32-tutorials/pic32mx220-tutorials/1-basic-digital-io-220
void main(void)
{
	long int count;
	float T, f;
	
	unsigned long cap = 0, ind = 0, res = 0;
	char buffer[20];
	
	CFGCON = 0;
	CNPUBbits.CNPUB0 = 1;
  
    UART2Configure(115200);  // Configure UART2 for a baud rate of 115200
    LCD_4BIT();
    
    ANSELAbits.ANSA1 = 1;   // set RA1 (AN0, pin 6 of DIP28) as analog pin
    TRISAbits.TRISA1 = 1;   // set RA1 as an input
    ANSELBbits.ANSB2 = 1;   // set RB2 (AN4, pin 7 of DIP28) as analog pin
    TRISBbits.TRISB2 = 1;   // set RB2 as an input
    
    ADCConf(); // Configure ADC
    
    ANSELB &= ~(1<<0); // Set RB0 as a digital I/O
    TRISB |= (1<<0);   // configure pin RB0 as input
    CNPUB |= (1<<0);   // Enable pull-up resistor for RB0
    
    ANSELB &= ~(1<<1); // Set RB1 as a digital I/O
    TRISB |= (1<<1);   // configure pin RB1 as input
    CNPUB |= (1<<1);   // Enable pull-up resistor for RB1
    
    ANSELB &= ~(1<<6); // Set RB6 as a digital I/O
    TRISB |= (1<<6);   // configure pin RB6 as input
    CNPUB |= (1<<6);   // Enable pull-up resistor for RB6
    
    ANSELB &= ~(1<<15); // Set RB15 as a digital I/O
    TRISB |= (1<<15);   // configure pin RB15 as input
    CNPUB |= (1<<15);   // Enable pull-up resistor for RB15
    
    ANSELB &= ~(1<<14); // Set RB14 as a digital I/O
    TRISB |= (1<<14);   // configure pin RB14 as input
    CNPUB |= (1<<14);   // Enable pull-up resistor for RB14
    
    ANSELB &= ~(1<<13); // Set RB13 as a digital I/O
    TRISB |= (1<<13);   // configure pin RB13 as input
    CNPUB |= (1<<13);   // Enable pull-up resistor for RB13
    
    ANSELB &= ~(1<<12); // Set RB12 as a digital I/O
    TRISB |= (1<<12);   // configure pin RB12 as input
    CNPUB |= (1<<12);   // Enable pull-up resistor for RB12
    
    ANSELB &= ~(1<<10); // Set RB10 as a digital I/O
    TRISB |= (1<<10);   // configure pin RB10 as input
    CNPUB |= (1<<10);   // Enable pull-up resistor for RB10
 
	waitms(500);
	printf("PIC32MX130 Period measurement using the core timer free running counter.\r\n"
	       "Connect signal to RB6 (pin 15).\r\n");
	       
    while(1)
    {	
    	if (IND_SWITCH)count = GetPeriod(100, 0);
    	else count = GetPeriod(100, 1);
    	
		if(count>0)
		{
			T=(count*2.0)/(SYSCLK*100.0);
			f=1/T;
			
			if (IND_SWITCH) {
				if (UNIT_SWITCH) {
				    cap = (unsigned long)((1.44f / (3000.0f * f)) * 1e12f);
				    
				    uint32_t adc_val = ADCRead(0); // Read LM335 on AN0
    				float temp_c = get_temperature_celsius(adc_val);
    				// 3. Apply compensation
    				float true_cap = apply_temp_compensation(cap, temp_c);
				    
				    cap = true_cap;
				    
			        sprintf(buffer, "%lu", cap);
			        strcat(buffer, "pF");
				} else {
				    cap = (unsigned long)((1.44f / (3000.0f * f)) * 1e9f);
				    
				    uint32_t adc_val = ADCRead(0); // Read LM335 on AN0
    				float temp_c = get_temperature_celsius(adc_val);
    				// 3. Apply compensation
    				float true_cap = apply_temp_compensation(cap, temp_c);
				    
				    cap = true_cap;
				    
			        sprintf(buffer, "%lu", cap);
			        strcat(buffer, "nF");
				}
				LCDprint("Cap: ", 1, 1);		
			    LCDprint(buffer, 2, 1);
			    
			} else if (RES_SWITCH) {
				if (UNIT_SWITCH) {
				    res = (unsigned long)((2000.0f * ADCRead(4))/(1023.0f - ADCRead(4)));
			        sprintf(buffer, "%lu", res);
			        strcat(buffer, "Ohms");
				} else {
				    res = (unsigned long)(((2000.0f * ADCRead(4))/(1023.0f - ADCRead(4)))*1e-3f);
			        sprintf(buffer, "%lu", res);
			        strcat(buffer, "KOhms");
				}
				LCDprint("Res: ", 1, 1);		
			    LCDprint(buffer, 2, 1);
			
			
			} else {
			    
				if (UNIT_SWITCH) {
				
					ind = (unsigned long) ((1.0f / (4.0f * 3.14159f * 3.14159f * f * f * 50.0f*1e-9f)) * 1.5e3f);
			        sprintf(buffer, "%lu", ind);
			        strcat(buffer, "mH");
				} else {
					ind = (unsigned long) ((1.0f / (4.0f * 3.14159f * 3.14159f * f * f * 50.0f*1e-9f)) * 1.5e6f);
			        sprintf(buffer, "%lu", ind);
			        strcat(buffer, "uH");
				}
				LCDprint("Ind: ", 1, 1);		
			    LCDprint(buffer, 2, 1);
			
			}
		    
		}
		else if (count == -1) printf("stuck high");
		else if (count == -2) printf("stuck low");
		
		waitms(200);
		
		if (SERIAL_SWITCH == 0) {
		
		    printf("\x1b[2J\x1b[H"); // ANSI: Clear from cursor to end of line.
		    
		    if (IND_SWITCH && UNIT_SWITCH) printf("c=%lupF", cap);
		    else if (IND_SWITCH) printf("c=%lunF", cap);
		    
		    else if (RES_SWITCH && UNIT_SWITCH) printf("r=%luOhms", res);
		    else if (RES_SWITCH) printf("r=%luKOhms", res);
		    
		    else if (UNIT_SWITCH) printf("l=%lumH", ind);
		    else printf("l=%luuH", ind);
		    
		    fflush(stdout); // GCC peculiarities: need to flush stdout to get string out without a '\n'
		
		} else if (SAVE_BUTTON == 0) {
			waitms(20);
			if (SAVE_BUTTON == 0) {
			
			    if (IND_SWITCH && UNIT_SWITCH) sprintf(buffer, "%lupFYNC\n", cap);
		        else if (IND_SWITCH) sprintf(buffer, "%lunFYNC\n", cap);
		        
		        else if (RES_SWITCH && UNIT_SWITCH) sprintf(buffer, "%luOhmsYNR\n", res);
		        else if (RES_SWITCH) sprintf(buffer, "%luKOhmsYNR\n", res);
		    
		        else if (UNIT_SWITCH) sprintf(buffer, "%lumHYNF\n", ind);
		        else sprintf(buffer, "%luuHYNF\n", ind);
			    
			    SerialTransmit(buffer);
				
				while (SAVE_BUTTON == 0);
				waitms(20);
			}
		} else if (PRINT_BUTTON == 0) {
			waitms(20);
			if (PRINT_BUTTON == 0) {
			
			    if (IND_SWITCH && UNIT_SWITCH) sprintf(buffer, "%lupFYYC\n", cap);
		        else if (IND_SWITCH) sprintf(buffer, "%lunFYYC\n", cap);
		        
		        else if (RES_SWITCH && UNIT_SWITCH) sprintf(buffer, "%luOhmsYYR\n", res);
		        else if (RES_SWITCH) sprintf(buffer, "%luKOhmsYYR\n", res);
		        
		        else if (UNIT_SWITCH) sprintf(buffer, "%lumHYYF\n", ind);
		        else sprintf(buffer, "%luuHYYF\n", ind);
			    
			    SerialTransmit(buffer);
				
				while (PRINT_BUTTON == 0);
				waitms(20);
			}
		} else {
		    if (IND_SWITCH && UNIT_SWITCH) sprintf(buffer, "%lupFNNC\n", cap);
		    else if (IND_SWITCH) sprintf(buffer, "%lunFNNC\n", cap);
		    
		    else if (RES_SWITCH && UNIT_SWITCH) sprintf(buffer, "%luOhmsNNR\n", res);
		    else if (RES_SWITCH) sprintf(buffer, "%luKOhmsNNR\n", res);
		        
		    else if (UNIT_SWITCH) sprintf(buffer, "%lumHNNF\n", ind);
		    else sprintf(buffer, "%luuHNNF\n", ind);
		    SerialTransmit(buffer);
		}
		
		
    }
}
