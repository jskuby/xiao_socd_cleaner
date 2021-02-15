/**
 * Seeeduino XIAO SOCD Cleaner
 * 
 * The SOCD algorithms take up to 80 cycles / 1.67μs to run. This can be reduced by removing the if statements and compiling
 * for a specific SOCD method.
 * 
 * DIP 1 sets the base operating mode between Neutral (OFF) and Second Input Priority (ON).
 * DIP 2 set to ON will make Up take priority.
 * 
 */

/**
 *  User Defines
 */

// Uncomment this define to show loop time and input/output states via serial monitoring
#define DEBUG

// Set the version of the prototype board being used - this controls the pin definitions in PINDEF.h
#define PROTOTYPE_VERSION 2

// Most controllers will use logic level LOW to trigger an input, however if you've separated your circuits with
// an optocoupler you will likely want to invert the output logic and trigger outputs with a HIGH value instead.
// Uncomment this define to invert the output logic.
// #define INVERT_OUTPUT_LOGIC

/**
 *  End User Defines
 */

#include "IODEF.h"

#define COMPARE_VALUE 65535

// Loop variables
// Set your selected SOCD cleaning method here: 0 - Neutral, 1 - Up Priority, 2 - Second Input Priority
Direction lastDirectionUD = Direction::neutral;
Direction lastDirectionLR = Direction::neutral;
bool secondInputPriority = false;
bool upPriority = false;

void configureSOCD() {
  uint32_t portA = PORT_IOBUS->Group[PORTA].IN.reg;
  uint32_t portB = PORT_IOBUS->Group[PORTB].IN.reg;

#if PROTOTYPE_VERSION == 1
  secondInputPriority = (portB & (1 << DIP1_PORT_PIN)) == 0;
  upPriority = (portB & (1 << DIP2_PORT_PIN)) == 0;
#else
  secondInputPriority = (portB & (1 << DIP1_PORT_PIN)) == 0;
  upPriority = (portA & (1 << DIP2_PORT_PIN)) == 0;
#endif
}

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
#endif
  configurePins();
  configureSOCD();
#ifdef DEBUG
  configureTimer();
  Serial.println("Setup complete, begin SOCD algorithm");
#endif
}

void loop() {
#ifdef DEBUG
  TC5->COUNT16.COUNT.reg = 0;
#endif
  uint32_t maskedInput = PORT_IOBUS->Group[PORTA].IN.reg ^ InputMasks::maskUDLR;
  uint32_t outputState = 0;

  if (maskedInput) {
    switch (maskedInput & InputMasks::maskUD) {
      case InputMasks::maskUD:
        if (upPriority)
          outputState |= InputMasks::valueU;
        else if (secondInputPriority)
          outputState |= (lastDirectionUD == Direction::up) ? InputMasks::valueD : InputMasks::valueU;
        break;
      case InputMasks::maskU:
        outputState |= InputMasks::valueU;
        if (secondInputPriority)
          lastDirectionUD = Direction::up;
        break;
      case InputMasks::maskD:
        outputState |= InputMasks::valueD;
        if (secondInputPriority)
          lastDirectionUD = Direction::down;
        break;
    }

    switch (maskedInput & InputMasks::maskLR) {
      case InputMasks::maskLR:
        if (secondInputPriority)
          outputState |= (lastDirectionLR == Direction::left) ? InputMasks::valueR : InputMasks::valueL;
        break;
      case InputMasks::maskL:
        outputState |= InputMasks::valueL;
        if (secondInputPriority)
          lastDirectionLR = Direction::left;
        break;
      case InputMasks::maskR:
        outputState |= InputMasks::valueR;
        if (secondInputPriority)
          lastDirectionLR = Direction::right;
        break;
    }
  }

#ifdef INVERT_OUTPUT_LOGIC
  PORT_IOBUS->Group[0].OUTCLR.reg = 0 | (outputState ^ InputMasks::valueUDLR);
  PORT_IOBUS->Group[0].OUTSET.reg = 0 | outputState;
#else
  PORT_IOBUS->Group[0].OUTCLR.reg = 0 | outputState;
  PORT_IOBUS->Group[0].OUTSET.reg = 0 | (outputState ^ InputMasks::valueUDLR);
#endif

#ifdef DEBUG
  // Log timing
  // Takes 8 cycles to reset timer
  Serial.println(TC5->COUNT16.COUNT.reg - 8);
  delay(100);
  // Serial.print("maskedInput: ");
  // Serial.println(maskedInput, BIN);
  // Serial.print("outputState: ");
  // Serial.println(outputState, BIN);
#endif
}

#ifdef DEBUG
void configureTimer() {
  // Enable generic clock for Timer/Counter 4 and 5
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID(GCM_TC4_TC5);
  while (GCLK->STATUS.bit.SYNCBUSY);

  // Perform software reset
  TC5->COUNT16.CTRLA.reg = TC_CTRLA_SWRST;
  while (TC5->COUNT16.STATUS.reg & TC_STATUS_SYNCBUSY);
  while (TC5->COUNT16.CTRLA.bit.SWRST);

  // Configure TC5
  TC5->COUNT16.CTRLA.reg =
    TC_CTRLA_MODE_COUNT16 |               // Counter of 16 bits
    TC_CTRLA_WAVEGEN_MFRQ |               // Match frequency
    TC_CTRLA_PRESCALER_DIV1;              // Prescaler of 1 (no division), 1 / 48000000 = 0.0000000208333 = 20.833ns/cycle | ~1.365ms window
  TC5->COUNT16.CC[0].reg = COMPARE_VALUE; // uint16_t max value - 1
  while (TC5->COUNT16.STATUS.bit.SYNCBUSY);

  // Start counter
  TC5->COUNT16.CTRLA.reg |= TC_CTRLA_ENABLE; // Enable TC5
  while (TC5->COUNT16.STATUS.bit.SYNCBUSY);
}
#endif