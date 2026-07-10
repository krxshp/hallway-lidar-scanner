#include <stdint.h>
#include "PLL.h"
#include "SysTick.h"
#include "uart.h"
#include "onboardLEDs.h"
#include "tm4c1294ncpdt.h"
#include "VL53L1X_api.h"
#include "stdio.h" // include respective .h files

#define I2C_MCR_MFE 0x00000010 // enable I2C

uint16_t dev = 0x29; // I2C device address of ToF sensor
int status = 0;

void I2C_Init(void){ // initialize I2C communication
    SYSCTL_RCGCI2C_R |= SYSCTL_RCGCI2C_R0;
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1; // enable GPIO port B clock
    while((SYSCTL_PRGPIO_R & 0x0002) == 0){};
    GPIO_PORTB_AFSEL_R |= 0x0C;
    GPIO_PORTB_ODR_R   |= 0x08;
    GPIO_PORTB_DEN_R   |= 0x0C;
    GPIO_PORTB_PCTL_R   = (GPIO_PORTB_PCTL_R & 0xFFFF00FF) + 0x00002200;
    I2C0_MCR_R  = I2C_MCR_MFE;
    I2C0_MTPR_R = 4; // calculated based on 10 MHz bus speed, results in 100kHz I2C communication speed
}

void PortM_Init(void){ // initialize Port M for buttons
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R11;
    while((SYSCTL_PRGPIO_R & SYSCTL_RCGCGPIO_R11) == 0){};
    GPIO_PORTM_DIR_R   &= ~0x03; // PM0 and PM1 as active LOW buttons
    GPIO_PORTM_DIR_R   |=  0x04; // PM2 to check bus speed with the AD3 
    GPIO_PORTM_AFSEL_R &= ~0x07;
    GPIO_PORTM_DEN_R   |=  0x07;
    GPIO_PORTM_PUR_R   |=  0x03; //pullup resistors on buttons
}

void PortJ_Init(void){ //initialize port J for PJ0 button (used same as PM0 button for testing
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R8;
    while((SYSCTL_PRGPIO_R & SYSCTL_RCGCGPIO_R8) == 0){};
    GPIO_PORTJ_DIR_R &= ~0x01;     
    GPIO_PORTJ_DEN_R |= 0x01;      
    GPIO_PORTJ_PUR_R |= 0x01;      
}

void PortF_Init(void){ // initialize Port F for LED
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R5;
    while((SYSCTL_PRGPIO_R & SYSCTL_RCGCGPIO_R5) == 0){};
    GPIO_PORTF_DIR_R   |= 0x11; // PF0, PF4 outputs
    GPIO_PORTF_DEN_R   |= 0x11;
}

void PortG_Init(void){
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R6;
    while((SYSCTL_PRGPIO_R & SYSCTL_PRGPIO_R6) == 0){};
    GPIO_PORTG_DIR_R   |= 0x01; // PH0 to PH3 outputs
    GPIO_PORTG_DEN_R   |= 0x01;
}


void PortH_Init(void){// initialize Port H for driver board for stepper motor
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R7;
    while((SYSCTL_PRGPIO_R & SYSCTL_RCGCGPIO_R7) == 0){};
    GPIO_PORTH_DIR_R   |= 0x0F; 
    GPIO_PORTH_DEN_R   |= 0x0F;
}

void PortN_Init(void){ // initialize Port N for LEDs
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R12;
    while((SYSCTL_PRGPIO_R & SYSCTL_RCGCGPIO_R12) == 0){};
    GPIO_PORTN_DIR_R   |= 0x03; // PN0, PN1 outputs
    GPIO_PORTN_DEN_R   |= 0x03;
}

void UART_printf_with_LED(char* message) { // function to handle UART transfer with LED flash (D3)
    GPIO_PORTN_DATA_R |= 0x01;
    UART_printf(message);
    GPIO_PORTN_DATA_R &= ~0x01;
}

void Signal_Clock_Pulse(void) { // Use to test assigned bus speed (make infinite while loop in main)
    GPIO_PORTM_DATA_R = 0x04;
    SysTick_Wait10ms(1);
    GPIO_PORTM_DATA_R = 0x00;
    SysTick_Wait10ms(1);
}

void Stepper_Step(uint8_t step, uint32_t delay) { // 4 phase steps for motor clockwise
    switch(step % 4) {
        case 0: GPIO_PORTH_DATA_R = 0b0011; break;
        case 1: GPIO_PORTH_DATA_R = 0b0110; break;
        case 2: GPIO_PORTH_DATA_R = 0b1100; break;
        case 3: GPIO_PORTH_DATA_R = 0b1001; break;
    }
    SysTick_Wait10ms(delay); // wait after each step to ensure mechanical movement catches up
}

void Stepper_Unwind(uint32_t delay) { //unwind motor steps in reverse for CCW
    for(int i = 0; i < 2048; i++) {
        switch(i % 4) {
            case 0: GPIO_PORTH_DATA_R = 0b1001; break;
            case 1: GPIO_PORTH_DATA_R = 0b1100; break;
            case 2: GPIO_PORTH_DATA_R = 0b0110; break;
            case 3: GPIO_PORTH_DATA_R = 0b0011; break;
        }
        SysTick_Wait10ms(delay);
    }
    GPIO_PORTH_DATA_R = 0x00; 
}

int PM0_Pressed(void) { // read PM0 active LOW button
    static uint8_t last = 0x01;
    uint8_t current = GPIO_PORTM_DATA_R & 0x01;
    if (last == 0x01 && current == 0x00) { //debounce loop (even if user presses for too long it'll work)
        SysTick_Wait10ms(2);
        last = 0x00; return 1;
    }
    last = current; return 0;
}

int PM1_Pressed(void) { // read PM1 active LOW button (same logic as PM0)
    static uint8_t last = 0x02;
    uint8_t current = GPIO_PORTM_DATA_R & 0x02;
    if (last == 0x02 && current == 0x00) {
        SysTick_Wait10ms(2);
        last = 0x00; return 1;
    }
    last = current; return 0;
}

int PJ0_Pressed(void) { // read PJ0 onboard button (same logic, not needed for usage)
    static uint8_t last = 0x01;
    uint8_t current = GPIO_PORTJ_DATA_R & 0x01;
    if (last == 0x01 && current == 0x00) {
        SysTick_Wait10ms(2);
        last = 0x00; return 1;
    }
    last = current; return 0;
}


int main(void){
    
		// Initialize all variables needed
		uint16_t Distance;
    uint8_t  dataReady = 0;
    uint32_t motorDelay = 1; // makes it so that motors delay by 10ms after each step
    int      sessionActive = 0; //session flag variable
    uint8_t  sensorState = 0;
    uint16_t wordData;
		
		// Initialize system
    PLL_Init();
    SysTick_Init();
    onboardLEDs_Init();
    UART_Init();
    I2C_Init();
    PortF_Init();
    PortG_Init();
    PortH_Init();
    PortM_Init();
    PortN_Init();
    PortJ_Init(); //not necessary
		
		// Initialize VL53L1X sensor
    status = VL53L1X_GetSensorId(dev, &wordData);
    while(sensorState == 0) status = VL53L1X_BootState(dev, &sensorState);
    VL53L1X_SensorInit(dev);
    VL53L1X_SetDistanceMode(dev, 2); // 2 means long distance mode (max of 3.6m)
    VL53L1X_StartRanging(dev);
    
    UART_printf_with_LED("Ready. PM0=Session, PM1/PJ0=Start Scan\r\n"); //tells PC that microcontroller is now ready
    FlashAllLEDs();
		
		// MAIN LOOP
    while(1){ // polling is used!
        if (PM0_Pressed()) {
            sessionActive = !sessionActive;
            if(sessionActive) {
                UART_printf_with_LED("SESSION_START\r\n");
                GPIO_PORTF_DATA_R |= 0x10; //special LED D3 on if session is active (enabled by PM0 button press)
            } else {
                UART_printf_with_LED("SESSION_END\r\n");
                GPIO_PORTF_DATA_R &= ~0x10; // LED D3 off if session ended
            }
        }

        if (!sessionActive) continue; //no scanning if session is not active
				
				
        if (PM1_Pressed() || PJ0_Pressed()) { //start scan
            UART_printf_with_LED("SCAN_START\r\n");

            for(int i = 0; i < 512; i++){
                for(int s = 0; s < 4; s++) Stepper_Step(s, motorDelay);

                if(i % 16 == 0){ // stops every 16 steps for a total of 32 distance measurements
                    GPIO_PORTN_DATA_R |= 0x02; 
                    
                    dataReady = 0;
                    while(dataReady == 0) VL53L1X_CheckForDataReady(dev, &dataReady);

                    VL53L1X_GetDistance(dev, &Distance);
                    VL53L1X_ClearInterrupt(dev);
                    
                    sprintf(printf_buffer, "%u\r\n", Distance);
                    UART_printf_with_LED(printf_buffer); //send distance to PC (happens ongoing)
                    
                    GPIO_PORTN_DATA_R &= ~0x02; //turn off scan LED
                }
            }

            UART_printf_with_LED("SCAN_END. RETRACTING...\r\n");
            Stepper_Unwind(motorDelay); //Rewind to original position with respective delays to prevent tangling in wires.
        }
    }
}