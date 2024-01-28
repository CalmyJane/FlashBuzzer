![FlashBuzzer Title](Images/title.png)


## Overview
This repo contains arduino code and documentation for a "FlashBuzzer". It's a simple industrial Big Red Button (Buzzer) with some additional buttons and dip switches attached to it. It's supposed to be rugged and installed at parties, where people can mash the button to affect lighting. There is an Output for WS2812 LED-Strips.

![Overview of the Buzzers inside.](Images/inside_full.jpg)

## Components

- Buzzer - Main Button to interact with.
- Red/Green Button - two additional pushbuttons to change values

![Buzzers red and green button.](Images/red_green_buttons.jpg)

- Program-Switches - 4 Dipswitches to input a 4-bit number (0..15) to select a mode or setting

![Buzzers Dipswitches.](Images/DIP_switches.jpg)

- Settings switch - another dipswitch to change between "run mode" and "setting mode"

## Run Mode

If the settings switch is low, the buzzer is in run mode, this is the intended mode for usage. Hitting the buzzer now does something, depending on the mode you are in:

Modes:
0 - Running Dot - When the buzzer is pressed, a single bright point is running down the LED strip. Color1 (Settings. 0=red, 1=green, 2=blue) is used for the color.
1 - Lightswitch Mode - Turns all LEDs on. Color1 is used for color.
2 - Pressure Bar - While holding the buzzer pressed, the bar gradually fills with Color1. After Exceeding Index1, all LEDs turn to Color2, after Exceeding Index 2, all LEDs turn to Color3.
3 -
4 - Counter Mode - Used to count LED-Lengths, use red/green button to turn on/off 10 more/less LEDs (+/- 10), use the buzzer to turn on single LEDs (+1)
5 -
6 -
7 -
8 -
9 -
10-
11-
12-
13-
14-
15- Debug Mode - All LEDs turn on, no matter of any settings or buttons

In Mode zero it makes a single flash run down the led strip.

## Settings Mode

If the settings switch is high, the buzzer is in settings mode, now the 4 Program-Switches select one of 16 settings. Each Mode can use any of these settings, there are some common shared one like colors or a general "speed" property, but every mode can interpret these values differently, be aware when setting this up.
Settings are stored on EEPROM

Settings:
- 0 - Color1 - Red
- 1 - Color1 - Green
- 2 - Color1 - Blue
- 3 - Speed
- 4 - LED-Count
- 5 - Index1
- 6 - Color2 - Red
- 7 - Color2 - Green
- 8 - Color2 - Blue
- 9 - Index2
- 10- Color3 - Red
- 11- Color3 - Green
- 12- Color3 - Blue
- 13- Random 0..255
- 14- Broken Mode 0..10
- 15- Broken Threshold 0..255


## Reset to Defaults

If something is not working follow these steps to reset the buzzer:
ToBeDone ;)
