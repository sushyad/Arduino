/*
 Copyright (C) 2011 James Coliz, Jr. <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
 */

/**
 * Example LED Remote
 *
 * This is an example of how to use the RF24 class to control a remote
 * bank of LED's using buttons on a remote control.
 *
 * On the 'remote', connect any number of buttons or switches from
 * an arduino pin to ground.  Update 'button_pins' to reflect the
 * pins used.
 *
 * On the 'led' board, connect the same number of LED's from an
 * arduino pin to a resistor to ground.  Update 'led_pins' to reflect
 * the pins used.  Also connect a separate pin to ground and change
 * the 'role_pin'.  This tells the sketch it's running on the LED board.
 *
 * Every time the buttons change on the remote, the entire state of
 * buttons is send to the led board, which displays the state.
 */

#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>
#include "printf.h"

//
// Hardware configuration
//

// Set up nRF24L01 radio on SPI bus plus pins 8 & 9

RF24 radio(7,3);

// Network uses that radio
RF24Network network(radio);

// Address of our node
const uint16_t this_node = 01;
const uint16_t base_node = 00;


// Pins on the remote for buttons
const int button_pins[] = { 2 };
const uint8_t num_button_pins = sizeof(button_pins);

// Pins on the LED board for LED's
const uint8_t led_pins[] = { 8, 9 };
const uint8_t num_led_pins = sizeof(led_pins);

int numOfStateChanges = 0;

//
// Topology
//

// Single radio pipe address for the 2 nodes to communicate.
const uint64_t pipe = 0xE8E8F0F0E1LL;

//
// Role management
//
// Set up role.  This sketch uses the same software for all the nodes in this
// system.  Doing so greatly simplifies testing.  The hardware itself specifies
// which node it is.
//
// This is done through the role_pin
//

//
// Payload
//

uint8_t button_states[num_button_pins];
uint8_t remote_button_states[num_button_pins];
uint8_t led_states[num_led_pins];

//
// Setup
//

void setup(void)
{
  //
  // Print preamble
  //

  Serial.begin(9600);
  printf_begin();
  printf("\n\rRemote Switch\n\r");

  //Attach the interrupt to the input pin and monitor for ANY Change
  attachInterrupt(0, stateChange, CHANGE);

  //
  // Setup and configure rf radio
  //

  SPI.begin();
  radio.begin();
  network.begin(/*channel*/ 90, /*node address*/ this_node);

  //
  // Open pipes to other nodes for communication
  //

  // This simple sketch opens a single pipes for these two nodes to communicate
  // back and forth.  One listens on it, the other talks to it.
  //radio.openReadingPipe(1,pipe);

  //
  // Start listening
  //

  //radio.startListening();

  //
  // Dump the configuration of the rf unit for debugging
  //

  //radio.printDetails();

  //
  // Set up buttons / LED's
  //

  // Set pull-up resistors for all buttons
//  int i = num_button_pins;
//  while(i--)
//  {
//    if (button_pins[i] > 0) {
//      pinMode(button_pins[i],INPUT);
//      digitalWrite(button_pins[i],HIGH);
//    }
//  }

  // Turn LED's ON until we start getting keys
  int i = num_led_pins;
  while(i--)
  {
    pinMode(led_pins[i],OUTPUT);
    led_states[i] = HIGH;
    digitalWrite(led_pins[i],led_states[i]);
  }

}

void stateChange()
{
  numOfStateChanges++;
  if (numOfStateChanges == 2) 
  {
    numOfStateChanges = 0;
    led_states[0] ^= HIGH;
    digitalWrite(led_pins[0],led_states[0]);
  }
  //delayMicroseconds(50);
}

//
// Loop
//

void loop(void)
{
  //
  // If the state of any button has changed, send the whole state of
  // all buttons.
  //

  // Pump the network regularly
  network.update();

  // Is there anything ready for us?
  while ( network.available() )
  {
    // If so, grab it and print it out
    RF24NetworkHeader header;
    network.read(header, remote_button_states, num_button_pins);
    
    // For each button, if the button now on, then toggle the LED
    int j = num_led_pins;

    while(j--)
    {
      if ( button_pins[j] > 0) {
        if (remote_button_states[j]) {
          led_states[j] ^= HIGH;
          digitalWrite(led_pins[j],led_states[j]);
        }
      }
    }
  }  

  // Try again in a short while

  // if there is data ready
//  if ( radio.available() )
//  {
//    // Dump the payloads until we've gotten everything
//    bool done = false;//  if ( !different )
//    while (!done)
//    {
//      // Fetch the payload, and see if this was the last one.
//      done = radio.read( remote_button_states, num_button_pins + 1);
//
//      // Spew it
//      printf("Got buttons\n\r");
//
//      // For each button, if the button now on, then toggle the LED
//      int j = num_led_pins;
//
//      while(j--)
//      {
//        if ( button_pins[j] > 0) {
//          if (remote_button_states[j]) {
//            led_states[j] ^= HIGH;
//            digitalWrite(led_pins[j],led_states[j]);
//          }
//        }
//      }
//    }
//  }
  delay(50);
}

