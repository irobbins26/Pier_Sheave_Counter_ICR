/** @file pier_sheave.ino
 *  This file contains code to read the low-resolution encoders on the Cal Poly
 *  Pier water column profiling gizmo's sheave. A pair of magnets produce 
 *  quadrature signals with somewhat sloppy phase; these signals are used to 
 *  figure out how far down the profiling instruments are, assuming no slippage
 *  of the cable on the sheave.
 *
 *  This program attempts to imitate the original from 2004 which ran on an 
 *  AT2313 or something ancient like that.
 *
 *  @author JR Ridgely with some help from GitHub Copilot AI
 *  @date   2025-03-18
 *  @copyright (c) 2025 by JRR, released under the LGPL V3.0
 */

#include <avr/interrupt.h>
#include <Arduino.h>

#undef DEBUG_IT                      // Only used when debugging via USB


/*volatile*/ int32_t position = 0;   ///< Current measured position of sheave
uint16_t errors = 0;                 ///< Number of encoder errors detected
int ENCODER_PIN_A = 2;               ///< Which digital pin goes to channel A
int ENCODER_PIN_B = 3;               ///< Which digital pin goes to channel B


/** This interrupt service routine is triggered when either of the encoder
 *  pins changes state. It compares the current values on the two channel
 *  pins to previous values and updates the measured position accordingly.
 */
void updateEncoder(void) 
// ISR(PCINT0_vect)
{
    static int lastEncoded = 0;

    uint8_t MSB = digitalRead(ENCODER_PIN_A);     // More significant bit
    uint8_t LSB = digitalRead(ENCODER_PIN_B);     // Less significant bit

    // Make a number with the previous pin values in bits 2 and 3 and the
    // current pin values in bits 0 and 1
    int encoded = (MSB << 1) | LSB;
    int sum = (lastEncoded << 2) | encoded;

    // Decode that value to determine which way the sheave has moved
    if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011)
    {
        position++;
    }
    else if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000)
    {
        position--;
    }
    else
    {
        errors++;
    }

    lastEncoded = encoded;            // Store this value for next time

#ifdef DEBUG_IT
    Serial.print('<');
    Serial.print(sum, BIN);
    Serial.println('>');
#endif
}


/** The Arduino setup function configures the serial port, sets up pins to read
 *  the encoder's two channels, and configures an interrupt to respond to
 *  changes of either channel's state.
 */
void setup(void)
{
    Serial.begin(9600);
    while (!Serial) { }
    Serial.flush();
    delay(1000);
    Serial.println("Encoder Counter V2.0");
    delay(1000);

    pinMode(ENCODER_PIN_A, INPUT);        // We have external pullups
    pinMode(ENCODER_PIN_B, INPUT);

    // The interrupt which checks for pin changes runs anytime a change is
    // detected on either the channel A or channel B line
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), updateEncoder, 
                                          CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), updateEncoder, 
                                          CHANGE);
}


/** The Arduino loop function communicates the currently measured position
 *  to the PC type computer which is controlling the profiler. It assumes
 *  that some other computer on the end of a serial communications line
 *  will ask it for position, check errors, or clear the position count.
 *
 *  - P means send position through serial port
 *  - E means send count of encoder errors (missed pulses or such)
 *  - C means clear position counter (set it to zero)
 */
void loop(void) 
{
    char command = '\0';              ///< First byte sent to the Arduino
    int32_t where_are_we;             ///< Extra copy of position
    uint16_t oopsies;                 ///< Extra copy of encoder errors

    if (Serial.available())
    {
        command = Serial.read();
        if (command == 'P' || command == 'p')       // Print position
        {
            // Disable interrupts and make a copy of the current position.
            // This guards against reading corrupted data if an interrupt
            // were to happen while we were getting the position.
            noInterrupts();
            where_are_we = position;
            interrupts();
            Serial.println(where_are_we);
        }
        else if (command == 'E' || command == 'e')  // Print errors
        {
            noInterrupts();
            oopsies = errors;
            interrupts();
            Serial.println(errors);
        }
        else if (command == 'C' || command == 'c')  // Zero position
        {
            noInterrupts();
            position = 0;
            errors = 0;
            interrupts();
        }
        else                      // The character sent was not understood
        {
            Serial.println('?');
        }
    }
#ifdef DEBUG_IT
    else
    {
        noInterrupts();
        where_are_we = position;
        interrupts();
        Serial.print(where_are_we);
        Serial.print(' ');
        delay(1000);
    }
#endif
}

