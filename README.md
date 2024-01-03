# FlashBuzzer

## Overview
This repo contains arduino code and documentation for a "FlashBuzzer". It's a simple industrial Big Red Button (Buzzer) with some additional buttons and dip switches attached to it. It's supposed to be rugged and installed at parties, where people can mash the button to affect lighting. There is an Output for WS2812 LED-Strips.

## Components

- Buzzer - Main Button to interact with.
- Red/Green Button - two additional pushbuttons to change values
- Program-Switches - 4 Dipswitches to input a 4-bit number (0..15) to select a mode or setting
- Settings switch - another dipswitch to change between "run mode" and "setting mode"

## Run Mode

If the settings switch is low, the buzzer is in run mode, this is the intended mode for usage. Hitting the buzzer now does something, depending on the mode you are in. In Mode zero it makes a single flash run down the led strip.

## Settings Mode

If the settings switch is high, the buzzer is in settings mode, now the 4 Program-Switches select one of 16 settings. Each Mode can use any of these settings, there are some common shared one like colors or a general "speed" property, but every mode can interpret these values differently, be aware when setting this up.
Settings are stored on EEPROM

## Reset to Defaults

If something is not working follow these steps to reset the buzzer:
ToBeDone ;)
