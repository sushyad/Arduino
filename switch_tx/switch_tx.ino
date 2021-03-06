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

RF24 radio(9,10);

// Network uses that radio
RF24Network network(radio);

// Address of our node
const uint16_t this_node = 00;
const uint16_t other_node[] = { 011, 01 };

// Pins on the remote for buttons
const uint8_t button_pins[] = { 7,8 };
const uint8_t num_button_pins = sizeof(button_pins);

// Pins on the LED board for LED's
const uint8_t led_pins[] = { 8,9 };
const uint8_t num_led_pins = sizeof(led_pins);

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

// The various roles supported by this sketch
typedef enum { role_remote, role_led = 1} role_e;

// The debug-friendly names of those roles
const char* role_friendly_name[] = { "Remote", "LED Board"};

// The role of the current running sketch
role_e role;

//
// Payload
//

uint8_t button_states[num_button_pins];
uint8_t led_states[num_led_pins];
uint8_t remote_button_state[1];

//
// Setup
//

void setup(void)
{
  role = role_remote;
  //
  // Print preamble
  //

  Serial.begin(9600);
  printf_begin();
  printf("\n\rRF24/examples/led_remote/\n\r");
  printf("ROLE: %s\n\r",role_friendly_name[role]);

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

//  if ( role == role_remote )
//  {
//    radio.openWritingPipe(pipe);
//  }
//  else
//  {
//    radio.openReadingPipe(1,pipe);
//  }
//
//  //
//  // Start listening
//  //
//
//  if ( role == role_led )
//    radio.startListening();
//
//  //
//  // Dump the configuration of the rf unit for debugging
//  //
//
//  radio.printDetails();

  //
  // Set up buttons / LED's
  //

  // Set pull-up resistors for all buttons
  if ( role == role_remote )
  {
    int i = num_button_pins;
    while(i--)
    {
      pinMode(button_pins[i],INPUT);
      digitalWrite(button_pins[i],HIGH);
    }
  }

  // Turn LED's ON until we start getting keys
  if ( role == role_led )
  {
    int i = num_led_pins;
    while(i--)
    {
      pinMode(button_pins[i],OUTPUT);
      led_states[i] = HIGH;
      digitalWrite(led_pins[i],led_states[i]);
    }
  }

}

//
// Loop
//

void loop(void)
{
  //
  // Remote role.  If the state of any button has changed, send the whole state of
  // all buttons.
  //

  if ( role == role_remote )
  {
    // Get the current state of buttons, and
    // Test if the current state is different from the last state we sent
    int i = num_button_pins;
    bool different = false;
    while(i--)
    {
      uint8_t state = ! digitalRead(button_pins[i]);
      if ( state != button_states[i] )
      {
        different = true;
        button_states[i] = state;
        printf("Now sending...");
        RF24NetworkHeader header(/*to node*/ other_node[i]);
        bool ok = network.write(header, button_states+i, 1);
  //      bool ok = radio.write( button_states, num_button_pins );
        if (ok)
          printf("ok\n\r");
        else
          printf("failed\n\r");
        }
    }

    network.update();
    
    // Is there anything ready for us?
    while ( network.available() )
    {
      // If so, grab it and print it out
      RF24NetworkHeader header;
      network.read(header, remote_button_state, 1);
      printf("Got button state: %u\n\r", remote_button_state[0]);
    } 
    
    // Try again in a short while
    delay(20);
  }

  //
  // LED role.  Receive the state of all buttons, and reflect that in the LEDs
  //

  if ( role == role_led )
  {
    // if there is data ready
    if ( radio.available() )
    {
      // Dump the payloads until we've gotten everything
      bool done = false;
      while (!done)
      {
        // Fetch the payload, and see if this was the last one.
        done = radio.read( button_states, num_button_pins );

        // Spew it
        printf("Got buttons\n\r");

        // For each button, if the button now on, then toggle the LED
        int i = num_led_pins;
        while(i--)
        {
          if ( button_states[i] )
          {
            led_states[i] ^= HIGH;
            digitalWrite(led_pins[i],led_states[i]);
          }
        }
      }
    }
  }
}
// vim:ai:cin:sts=2 sw=2 ft=cpp
