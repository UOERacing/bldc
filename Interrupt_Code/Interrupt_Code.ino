#include "msp430f5529.h"
#include <SPI.h>

/*
   UOE Racing: BLDC Motor Controller Program 1.1.0

   CONTRIBUTORS:
     - Atinderpaul Kanwar - ap_kanwar@outlook.com
     - Ryan Fleck - Ryan.Fleck@protonmail.com

   CHANGELOG:
   Version 1.1.2:
     - Update pins for UOE 2020rA board.
     - Temporarily disable other SPI devices.

   Version 1.1.1:
     - Add initMotorState function to read halls during setup.
     - FIX TODO: Jumpstart motor on throttle.
     - Add red running-light to indicate program is ready.
     - Remove next-state code, no longer used.

   Version 1.1.0:
     - Change pins to interrupt-capable pins.
     - Reverse sequence of hall status modification operations.

   Version 1.0.0:
     - Initial implementation of interrupt code.

*/


/*
   UOE 2020rA BOARD SETUP INSTRUCTIONS:

   Yellow bundle:
   A -> White wire.
   B -> Purple wire.
   C -> Orange wire.
*/

#define ENABLE 5


// SPI PINS
#define SCLK 7
#define nFAULT 33

#define CS_TEMP 32
#define CS_SD 11
#define EXTERNAL_SPI_ENABLE 15

#define CS_DRV 8

// HALL PINS
#define HALLA 19
#define HALLB 13
#define HALLC 12

#define LOWA 39
#define LOWB 37
#define LOWC 35

#define HIGHA 40
#define HIGHB 38
#define HIGHC 36

#define THROTTLE 24

// Constants
#define PWM_VALUE_DEBUG 128 // During testing, this value is used as throttle while the MSP430 buttons are pressed.
#define PWM_FREQUENCY 20000 // 20kHz
#define DEFAULT_THROTTLE_LOOP_COUNT 1000 // must be smaller than 65,535
#define MIN_PWM_VALUE 0
#define MAX_PWM_VALUE 255

//State Variable Values
#define zeroDegrees B110 //6
#define twentyDegrees B010 //2
#define fortyDegrees B011 //3
#define sixtyDegrees B001 //1
#define eightyDegrees B101 //5
#define hundredDegrees B100 //4
#define errorState1 B111
#define errorState2 B000

// Function call repitition macros
#define postInterruptAction() motorSpin()

//Throttle Variables and Pin
int loopCount = 0; //Variable to store number of loops gone through
int loopNum = DEFAULT_THROTTLE_LOOP_COUNT; //Number of loops to trigger a throttle read
int pwm_value = 0; //Throttle value, always start this off at 0
volatile byte state = 0;



/*
   SETUP CODE
     - Configures MSP430 pins
     - Calls function to set up SPI
     - Sets interrupts
*/
void setup() {

  // Disable other SPI devices:
  pinMode(CS_TEMP, OUTPUT);
  pinMode(CS_SD, OUTPUT);
  pinMode(EXTERNAL_SPI_ENABLE, OUTPUT);
  digitalWrite(CS_TEMP, HIGH);
  digitalWrite(CS_SD, HIGH);
  digitalWrite(EXTERNAL_SPI_ENABLE, HIGH);

  digitalWrite(ENABLE, HIGH);
  pinMode(ENABLE, OUTPUT);
  digitalWrite(ENABLE, HIGH);


  delayMicroseconds(2);
  digitalWrite(ENABLE, LOW); // Keep enable low for between 4 and 40 microseconds
  delayMicroseconds(20);
  digitalWrite(ENABLE, HIGH);

  digitalWrite(CS_DRV, HIGH);
  pinMode(CS_DRV, OUTPUT);
  digitalWrite(CS_DRV, HIGH);

  digitalWrite(SCLK, LOW);
  pinMode(SCLK, OUTPUT);
  digitalWrite(SCLK, LOW);

  pinMode(nFAULT, INPUT);

  pinMode(HALLA, INPUT);
  pinMode(HALLB, INPUT);
  pinMode(HALLC, INPUT);

  digitalWrite(LOWA, LOW);
  digitalWrite(LOWB, LOW);
  digitalWrite(LOWC, LOW);
  digitalWrite(HIGHA, LOW);
  digitalWrite(HIGHB, LOW);
  digitalWrite(HIGHC, LOW);
  pinMode(LOWA, OUTPUT);
  pinMode(LOWB, OUTPUT);
  pinMode(LOWC, OUTPUT);
  pinMode(HIGHA, OUTPUT);
  pinMode(HIGHB, OUTPUT);
  pinMode(HIGHC, OUTPUT);

  // Input from steering wheel
  pinMode(THROTTLE, INPUT);

  // Lights and switches for debugging
  pinMode(P1_1, INPUT_PULLUP);
  pinMode(P2_1, INPUT_PULLUP);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  Serial.begin(9600);

  analogFrequency(PWM_FREQUENCY);
  attachInterrupt(digitalPinToInterrupt(HALLA), changeSA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(HALLB), changeSB, CHANGE);
  attachInterrupt(digitalPinToInterrupt(HALLC), changeSC, CHANGE);
  DRV8323_SPI_Setup();

  // TODO: Remove and see if things still work.
  interrupts();

  // Set initial state.
  initMotorState();

  // Red LED indicates program is ready to roll!

  IncreaseClockSpeed_25MHz();
  digitalWrite(RED_LED, HIGH);
} // setup() END



/*
   LOOP CODE
*/
volatile int button1_state = 1;
volatile int button2_state = 1;

void loop() {
  delay(10);
  //
  //  if (!kickstarted) {
  //    kickstarted = 1;
  //    delay(100);
  //    motorSpin();
  //  }

  // Button DOWN will be read as 0, not 1!
  button1_state = digitalRead(P1_1);
  button2_state = digitalRead(P2_2);


  if (!button1_state) {
    digitalWrite(GREEN_LED, HIGH);
    motorSpin();
  } else {
    digitalWrite(GREEN_LED, LOW);
  }

  //  if (!button2_state) {
  //    pwm_value = 256;
  //  } else {
  //    pwm_value = PWM_VALUE_DEBUG;
  //  }


  // Temporary logic using either built-in button on the MSP430 board to run the motor.
  //  if (button2_state && button1_state) {
  //    //If neither button has been pressed, throttle can be set to zero.
  //    pwm_value = PWM_VALUE_DEBUG;
  //    digitalWrite(GREEN_LED, LOW);
  //  } else {
  //    // If one or the other is pressed, the bitwise AND will be zero, so throttle should be set.
  //    pwm_value = PWM_VALUE_DEBUG;
  //    digitalWrite(GREEN_LED, HIGH);
  //    motorSpin();
  //  }

  //  // Reading and Updating the Throttle the Throttle
  //  if(loopCount >= loopNum) {
  //    int analogReadValue = analogRead(THROTTLE); // Read the POT Value
  //    int mapValue = map(analogReadValue, 1550, 4096, 0, 255); // Map the pot value, assuming that the range is between 1550-4096 (12-bit value) to 0-255
  //
  //    // MIN and MAX Check
  //    if (mapValue <= MIN_PWM_VALUE) {
  //      mapValue == MIN_PWM_VALUE;
  //    }
  //    if (mapValue > MAX_PWM_VALUE) {
  //      pwm_value = MAX_PWM_VALUE;
  //    }
  //    else {
  //      pwm_value = (mapValue + pwm_value) / 2; // The PWM value keeps increasing at a rate of 2
  //    }
  //    loopCount=0;
  //  }
  //  //If you havn't reached loopNum then increment and continue to the next loop
  //  else{
  //    loopCount++;
  //  }

} // loop() END



/*
   SPI SETUP FUNCTION
*/
void DRV8323_SPI_Setup(void) {
  delay(200); // Why is this here?
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV64);
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE1);
  SPI.setModule(0);

  // Writing to Control Register
  // Selecting 3xPWM Mode Function, using controller driver control register:
  // BIT#  15  14 13 12 11 10    09        08            07    06 05     04    03        02    01   00
  // Name  R/W < Addr3-0 > res chgpump gatedrivevolt overtemp pwmMode  pwmcom 1pwmdir  coast brake clearfault
  //       W=0 (MSB first)
  // Value  0  Ctrl=0010    0     0         0             0   3xPWM=01    0     0         0     0    0
  // This corresponds to a 16-bit value to put motor controller into 3xPWM mode: 0(001 0)000 0(01)0 0000 => 0x1020
  // This is actually executed as single-byte writes of 0x10 then 0x20

  digitalWrite(CS_DRV, LOW); //Pull slave select low to begin the transaction
  delayMicroseconds(2);
  SPI.transfer(0x10);
  byte return_value = SPI.transfer(0x20);
  Serial.println(return_value, HEX);
  Serial.println();
  delayMicroseconds(2);
  digitalWrite(CS_DRV, HIGH); //Pull slave select high to end the transaction

  SPI.end();
}


/*
   MOTOR SPIN FUNCTION
     - Called after an interrupt has been encountered
     - Writes new values to the HALLs
*/
void motorSpin() {
  switch (state) {
    case zeroDegrees: //010
      //setLow('B');
      digitalWrite(LOWB, HIGH);
      digitalWrite(HIGHB, LOW);
      //setHighZ('A');
      digitalWrite(LOWA, LOW);
      digitalWrite(HIGHA, LOW);
      //setHigh('C');
      digitalWrite(LOWC, HIGH);
      analogWrite(HIGHC, pwm_value);
      break;

    case twentyDegrees: //011
      //setLow('B');
      digitalWrite(LOWB, HIGH);
      digitalWrite(HIGHB, LOW);
      //setHighZ('C');
      digitalWrite(LOWC, LOW);
      digitalWrite(HIGHC, LOW);
      //setHigh('A');
      digitalWrite(LOWA, HIGH);
      analogWrite(HIGHA, pwm_value);
      break;

    case fortyDegrees: //001
      //setLow('C');
      digitalWrite(LOWC, HIGH);
      digitalWrite(HIGHC, LOW);
      //setHighZ('B');
      digitalWrite(LOWB, LOW);
      digitalWrite(HIGHB, LOW);
      //setHigh('A');
      digitalWrite(LOWA, HIGH);
      analogWrite(HIGHA, pwm_value);
      break;

    case sixtyDegrees: //101
      //setLow('C');
      digitalWrite(LOWC, HIGH);
      digitalWrite(HIGHC, LOW);
      //setHighZ('A');
      digitalWrite(LOWA, LOW);
      digitalWrite(HIGHA, LOW);
      //setHigh('B');
      digitalWrite(LOWB, HIGH);
      analogWrite(HIGHB, pwm_value);
      break;

    case eightyDegrees: //100
      //setLow('A');
      digitalWrite(LOWA, HIGH);
      digitalWrite(HIGHA, LOW);
      //setHighZ('C');
      digitalWrite(LOWC, LOW);
      digitalWrite(HIGHC, LOW);
      //setHigh('B');
      digitalWrite(LOWB, HIGH);
      analogWrite(HIGHB, pwm_value);
      break;

    case hundredDegrees: //110
      //setLow('A');
      digitalWrite(LOWA, HIGH);
      digitalWrite(HIGHA, LOW);
      //setHighZ('B');
      digitalWrite(LOWB, LOW);
      digitalWrite(HIGHB, LOW);
      //setHigh('C');
      digitalWrite(LOWC, HIGH);
      analogWrite(HIGHC, pwm_value);
      break;

    default :
      coast();
      break;

  } // SWITCH END
}// motorSpin() END



/*
   COAST FUNCTION
     - Removes all forces from the BLDC by disabling FETs, allowing the motor to coast
*/
void coast (void) {
  //setHighZ('A');
  digitalWrite(LOWA, LOW);
  digitalWrite(HIGHA, LOW);
  //setHighZ('B');
  digitalWrite(LOWB, LOW);
  digitalWrite(HIGHB, LOW);
  //setHighZ('C');
  digitalWrite(LOWC, LOW);
  digitalWrite(HIGHC, LOW);
}



/*
   INITMOTORSTATE FUNCTION
     - Checks the halls and sets the state byte.
*/
void initMotorState() {
  state = B000;
  state |= digitalRead(HALLA) ? B100 : B000;
  state |= digitalRead(HALLB) ? B010 : B000;
  state |= digitalRead(HALLC) ? B001 : B000;
}



/*
   INTERRUPT FUNCTIONS:
   changeSX: Interrupt sequence for HALL X.
     Each interrupt function is triggered during a voltage change on the relevant
     pin.
*/
void changeSA() {
  if (digitalRead(HALLA) == HIGH) {
    state |= B100;
  }
  else {
    state &= B001;
  }
  postInterruptAction();
}

void changeSB() {
  if (digitalRead(HALLB) == HIGH) {
    state |= B010;
  }
  else {
    state &= B100;
  }
  postInterruptAction();
}

void changeSC() {
  if (digitalRead(HALLC) == HIGH) {
    state |= B001;
  }
  else {
    state &= B010;
  }
  postInterruptAction();
}



/*
   CPU Tuning Functions

   SetVCoreUp - Increases core voltage to allow higher clock speed.

   IncreaseClockSpeed_25MHz - Increases clock speed incrementally to 25 MHz.
*/
void SetVcoreUp (unsigned int level)
{
  /*
     From TI example MSP430F55xx_UCS_10.c
     Increses CPU voltage.
  */

  // Open PMM registers for write
  PMMCTL0_H = PMMPW_H;

  // Set SVS/SVM high side new level
  SVSMHCTL = SVSHE + SVSHRVL0 * level + SVMHE + SVSMHRRL0 * level;

  // Set SVM low side to new level
  SVSMLCTL = SVSLE + SVMLE + SVSMLRRL0 * level;

  // Wait till SVM is settled
  while ((PMMIFG & SVSMLDLYIFG) == 0);

  // Clear already set flags
  PMMIFG &= ~(SVMLVLRIFG + SVMLIFG);

  // Set VCore to new level
  PMMCTL0_L = PMMCOREV0 * level;

  // Wait till new level reached
  if ((PMMIFG & SVMLIFG))
    while ((PMMIFG & SVMLVLRIFG) == 0);

  // Set SVS/SVM low side to new level
  SVSMLCTL = SVSLE + SVSLRVL0 * level + SVMLE + SVSMLRRL0 * level;

  // Lock PMM registers for write access
  PMMCTL0_H = 0x00;
}

void IncreaseClockSpeed_25MHz()
{
  /* Increase Vcore one step at a time. */
  SetVcoreUp (0x01);
  SetVcoreUp (0x02);
  SetVcoreUp (0x03);

  UCSCTL3 = SELREF_2; // Set DCO FLL reference = REFO
  UCSCTL4 |= SELA_2; // Set ACLK = REFO

  __bis_SR_register(SCG0); // Disable the FLL control loop
  UCSCTL0 = 0x0000; // Set lowest possible DCOx, MODx
  UCSCTL1 = DCORSEL_7; // Select DCO range 50MHz operation
  UCSCTL2 = FLLD_0 + 762; // Set DCO Multiplier for 25MHz

  /**
     (N + 1) * FLLRef = Fdco
     (762 + 1) * 32768 = 25MHz
     Set FLL Div = fDCOCLK/2

     Worst-case settling time for the DCO when the DCO range bits have been
     changed is n x 32 x 32 x f_MCLK / f_FLL_reference. See UCS chapter in 5xx
     UG for optimization.
     32 x 32 x 25 MHz / 32,768 Hz ~ 780k MCLK cycles for DCO to settle
  */
  __bic_SR_register(SCG0); // Enable the FLL control loop
  __delay_cycles(782000);

  // Loop until XT1,XT2 & DCO stabilizes
  // -> In this case only DCO has to stabilize
  do
  {
    // Clear XT2,XT1,DCO fault flags
    UCSCTL7 &= ~(XT2OFFG + XT1LFOFFG + DCOFFG);

    // Clear fault flags
    SFRIFG1 &= ~OFIFG;

  } while (SFRIFG1 & OFIFG); // Test oscillator fault flag
}
