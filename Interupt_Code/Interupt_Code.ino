#include <SPI.h>

#define ENABLE 5

// SPI PINS
#define nSCS 12
#define SCLK 7
#define nFAULT 8
#define MISO 14
#define MOSI 15

// HALL PINS
#define HALLA 19
#define HALLB 18
#define HALLC 13

#define LOWA 39
#define LOWB 37
#define LOWC 35
#define HIGHA 40
#define HIGHB 38
#define HIGHC 36

#define LED RED_LED

#define THROTTLE 24

// Constants
#define PWM_VALUE 100
#define PWM_FREQUENCY 20000 // 20kHz
#define DEFAULT_THROTTLE_LOOP_COUNT 1000 // must be smaller than 65,535
#define MIN_PWM_VALUE 0
#define MAX_PWM_VALUE 255

//Throttle Variables and Pin
int loopCount = 0; //Variable to store number of loops gone through 
int loopNum = DEFAULT_THROTTLE_LOOP_COUNT; //Number of loops to trigger a throttle read
int pwm_value = 120; //Throttle value, always start this off at 0

int ledstatus = 0;
int ledtoggle = -1;

//State Variable Values
#define zeroDegrees B110 //6
#define twentyDegrees B010 //2
#define fortyDegrees B011 //3
#define sixtyDegrees B001 //1
#define eightyDegrees B101 //5
#define hundredDegrees B100 //4
#define errorState1 B111
#define errorState2 B000

byte state = 0;
byte old_state = state;
byte expected_next_state = state;

int desiredTorque = 0;
bool new_state_OK = false;

/* Coast
*Allows the motor to coast by turning off all Fets
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

// SPI SETUP
void controllerSetup(void) {
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

  digitalWrite(nSCS, LOW); //Pull slave select low to begin the transaction
  delayMicroseconds(2);
  SPI.transfer(0x10);
  byte return_value = SPI.transfer(0x20);
  Serial.println(return_value, HEX);
  Serial.println();
  delayMicroseconds(2);
  digitalWrite(nSCS, HIGH); //Pull slave select high to end the transaction
  
  SPI.end();
}

void motorSpin() {
  // Switch-Case Statement follow the REVERSE1 code, (which previously required a push to start spinning the motor clockwise    (Check clockwise_motor brach on GITLAB)
  switch(state) {
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
      expected_next_state = twentyDegrees;
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
      expected_next_state = fortyDegrees;
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
      expected_next_state = sixtyDegrees;
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
      expected_next_state = eightyDegrees;
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
      expected_next_state = hundredDegrees;
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
      expected_next_state = zeroDegrees;
    break;
  
    default :
      Serial.println("Error - Set to Coast");
      Serial.println(state);
      coast();
      expected_next_state = state;  // assume the same errored state is the most likely one next
    break;
  } // SWITCH END
}

// SETUP CODE
void setup() {
  digitalWrite(ENABLE, HIGH);
  pinMode(ENABLE, OUTPUT);
  digitalWrite(ENABLE, HIGH);
  delayMicroseconds(2);
  digitalWrite(ENABLE, LOW); // Keep enable low for between 4 and 40 microseconds
  delayMicroseconds(20);
  digitalWrite(ENABLE, HIGH);

  digitalWrite(nSCS, HIGH);
  pinMode(nSCS, OUTPUT);
  digitalWrite(nSCS, HIGH);

  digitalWrite(SCLK, LOW);
  pinMode(SCLK, OUTPUT);
  digitalWrite(SCLK, LOW);
  
  pinMode(nFAULT, INPUT);
  // pinMode(MISO, INPUT_PULLUP);
  pinMode(MISO, INPUT);
  pinMode(MOSI, OUTPUT);

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

  pinMode(THROTTLE, INPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(P1_1, INPUT_PULLUP);
  pinMode(P2_1, INPUT_PULLUP);
  pinMode(GREEN_LED, OUTPUT);

  state = errorState1;
  old_state = errorState2;
  expected_next_state = errorState2;
  
  Serial.begin(9600);

  analogFrequency(PWM_FREQUENCY);
  attachInterrupt(digitalPinToInterrupt(HALLA), changeSA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(HALLB), changeSB, CHANGE);
  attachInterrupt(digitalPinToInterrupt(HALLC), changeSC, CHANGE); 
  controllerSetup();

  // SET THE INITIAL STATE OF THE HALL INPUTS
  //Read HALL Values
  int rHallA = digitalRead(HALLA);
  int rHallB = digitalRead(HALLB);
  int rHallC = digitalRead(HALLC);

  if(rHallA == HIGH) {
    state = state | B100;  // set bit 2
  }
  if(rHallB == HIGH) {
    state = state | B010;  // set bit 1
  }
  if (rHallC == HIGH) {
    state = state | B001;  // set bit 0
  }

  // Ensure red LED toggle works correctly
  // for interrupt debugging.
  noInterrupts();
  toggleLed(-3);
  delay(500);
  toggleLed(-2);
  interrupts();

  motorSpin();
}

// LOOP CODE
void loop() {

    // Temporary logic using either button on the board to run the motor.
  if (digitalRead(P1_1) & digitalRead(P2_1)) {
    //If neither button has been pressed, throttle can be set to zero.
    //pwm_value = 0;
    digitalWrite(GREEN_LED, LOW);
  } else {
    // If one or the other is pressed, the bitwise AND will be zero, so throttle should be set.
    //pwm_value = PWM_VALUE;
    digitalWrite(GREEN_LED, HIGH);
  }

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

} // LOOP END

void changeSA() {
  toggleLed(1);
  if(digitalRead(HALLA) == HIGH) {
    state = state | B100;
  }
  else {
    state = state & B010;
  }

  motorSpin();
  //digitalWrite(RED_LED, LOW);
}

void changeSB() {
  toggleLed(2);
  if(digitalRead(HALLB) == HIGH) {
    state = state | B010;
  }
  else {
    state = state & B001;
  }

  motorSpin();
}

void changeSC() {
  toggleLed(3);
  if(digitalRead(HALLC) == HIGH) {
    state = state | B001;
  }
  else {
    state = state & B100;
  }

  motorSpin();
}

/*
 * toggleLed
 * 
 * Enables/disables the red LED based on int status.
 */
void toggleLed(int x){
  // Every time a new int is entered, the LED toggles.
  if(x != ledtoggle){
    ledtoggle = x;
    if(ledstatus){
        digitalWrite(RED_LED, LOW);
        ledstatus = 0;
    }else{
        digitalWrite(RED_LED, HIGH);
        ledstatus = 1;
    }
  }
}
