#include <FastLED.h>
#include <EEPROM.h>

#define LED_PIN     3
#define NUM_LEDS    255
#define LED_TYPE    WS2811
#define COLOR_ORDER RGB

int* readFromEEPROM() {
    static int data[16];  // Making it static to retain its value after the function returns
    for (int i = 0; i < 16; i++) {
        data[i] = EEPROM.read(i);
    }
    return int(255, 255, 255, 255, 1024, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ,0);
    return data;
}

// CRGB leds[NUM_LEDS];
CRGB leds[NUM_LEDS]; //Initialize led array with nubmer of elements from eeprom

#define BUTTON_PIN 4
#define RED_BTN    2
#define GRN_BTN    A5
#define DIP1       A4
#define DIP2       A3
#define DIP3       A2
#define DIP4       A1
#define DIP_SET    A0

enum LEDSetting {
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    SPEED,
    NUMBER_LEDS,
    RESERVED1,
    RESERVED2,
    RESERVED3,
    RESERVED4,
    RESERVED5,
    RESERVED6,
    RESERVED7,
    RESERVED8,
    RESERVED9,
    RESERVED10,
    RESERVED11
    // Add other settings as needed
};

class LEDSettingsManager {

  //Settings:
  // 0 Color Red - 0=Random Color, 1-254 = fixed color, 255 = white
  // 1 Color Green - 0..255
  // 2 Color Blue - 0..255
  // 3 Speed - Speed from 0 (slow) to 255 (fast)
  // 4 Number Leds - 50..255, when this is changed, the device needs to be reset!
  //rest is reserved
private:
    int setts[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int displayStartIndex = 0; // Start index for display settings
    int displayEndIndex = 44; // End index for display settings
    bool toggle = false; //used to blink the LEDs

public:
    LEDSettingsManager() {
        // Initialize settings array
      read_from_eeprom();
    }

    void read_from_eeprom() {
      for (int i = 0; i < 16; i++) {
          setts[i] = EEPROM.read(i);
      }
    }

    bool is_dark(){
      return (setts[COLOR_RED] <= 5 && setts[COLOR_GREEN] <= 5 && setts[COLOR_BLUE] <= 5);
    }

    CRGB get_color(){
      return CRGB(setts[COLOR_RED], setts[COLOR_GREEN], setts[COLOR_BLUE]);
    }

    int get_speed(){
      return setts[SPEED];
    }

    void display_setting(LEDSetting setting_index, bool blinking=false) {
        // Show current setting value as solid LEDs
        int value = map(setts[setting_index], 0, 255, displayStartIndex, displayEndIndex);
        CRGB bar_color = CRGB::Red;
        if(setting_index == NUMBER_LEDS){
          //changing Number of LEDs
          value = setts[NUMBER_LEDS]; //use value unscaled, so the length of the LED strip can be adjusted from 0 .. 255
          bar_color = CRGB::Blue;
        }
        if(setting_index >= 0 && setting_index <= 2){
          //setting color, use current color to display
          if(setts[0] <= 5 && setts[1] <= 5 && setts[2] <= 5){
            //all Colors low, choose random
            bar_color = CRGB(random(255), random(255), random(255));
          } else if(setts[0] == 255) {
            bar_color = CRGB(255, 255, 255);
          } else{
            bar_color = CRGB(setts[0], setts[1], setts[2]);
          }
        }

        //Display LED-Bar
        for (int i = 0; i < NUM_LEDS; i++) {
            if (i >= (displayStartIndex) && i <= value) {
                leds[i] = toggle || !blinking ? bar_color : CRGB::Black;
            } else {
                leds[i] = CRGB::Black;
            }
        }          
        
        toggle = !toggle;
        delay(map(setts[SPEED], 0, 255, 100, 50)); //blink with current speed setting
        // Note: FastLED.show() should be called externally after this function
    }

    void set_setting(int setting_index, int value) {
        if(setting_index == 5 && value < 30){
          //limit number of LEDs to 
          value = 30;
        }
        setts[setting_index] = value;
        display_setting(setting_index, true); // Update display to show new setting
        // Note: FastLED.show() should be called externally after this function
    }

    int get_setting(int setting_index) {
        return setts[setting_index];
    }

    int* get_settings() {
        return setts;
    }

    void write_to_eeprom(){
      //writes the current settings to EEPROM
      for (int i = 0; i < 16; i++) {
        EEPROM.write(i, setts[i]);
      }
      Serial.println("written to EEPROM");
    }

    // Additional methods and properties can be added as needed
};

class LEDMode {
public:
    virtual void update(LEDSettingsManager settings) = 0;
};

class RunningDotMode : public LEDMode {
private:
    bool oldButtonState = HIGH;

public:
    void update(LEDSettingsManager settings) override {
        bool newButtonState = digitalRead(BUTTON_PIN);
        if (newButtonState == LOW && oldButtonState == HIGH) {
            CRGB color = settings.is_dark() ? CRGB(random(255), random(255), random(255)) : settings.get_color(); // Random or white color if no color selected
            leds[0] = color;
        }
        oldButtonState = newButtonState;

        for (int i = NUM_LEDS - 1; i > 0; i--) {
            leds[i] = leds[i - 1];
        }
        leds[0] = CRGB::Black;

        delay(map(settings.get_speed(), 0, 255, 300, 3)); // Speed of shifting
    }
};

class LightSwitchMode : public LEDMode {
private:
    bool isOn = false;
    bool oldButtonState = HIGH;

public:
    void update(LEDSettingsManager settings) override {
        bool newButtonState = !digitalRead(BUTTON_PIN);
        if (newButtonState == LOW && oldButtonState == HIGH) {
            isOn = !isOn;
            CRGB color = isOn ? (settings.is_dark() ? CRGB(random(255), 255, 255) : settings.get_color()) : CRGB::Black; // Random or white color based on parameter
            fill_solid(leds, NUM_LEDS, color);
            delay(20); // Debounce delay
        } else {
          //avoid super fast looping
          delay(20);
        }
        oldButtonState = newButtonState;
    }
};

class GradualFillMode : public LEDMode {
private:
    int fillLevel = 0;
    bool filling = false;

public:
    void update(LEDSettingsManager settings) override {
        if (digitalRead(BUTTON_PIN) == LOW) {
            filling = true;
        } else {
            filling = false;
        }

        if (filling && fillLevel < NUM_LEDS) {
            fillLevel++;
        } else if (!filling && fillLevel > 0) {
            fillLevel--;
        }

        CRGB color = calculateUniformColor(fillLevel);
        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = i < fillLevel ? color : CRGB::Black;
        }
        delay(10);
    }

    CRGB calculateUniformColor(int fillLevel) {
        float ratio = float(fillLevel) / float(NUM_LEDS);
        if (ratio < 0.33) {
            return CRGB::Green; // Green when few LEDs are lit
        } else if (ratio < 0.66) {
            return blend(CRGB::Green, CRGB::Red, (ratio - 0.33) * 6.0); // Blend to yellow and then to red
        } else {
            return CRGB::Red; // Red when many LEDs are lit
        }
    }
};

class GameMode : public LEDMode {
public:
    void update(LEDSettingsManager settings) override {
        // Placeholder for game mode
        // Implement game logic here
    }
};

class BTN_INPUT {
public:
    BTN_INPUT(uint8_t incPin, uint8_t decPin) : incrementPin(incPin), decrementPin(decPin), incrementButtonState(LOW), decrementButtonState(LOW) {
        pinMode(incPin, INPUT);
        pinMode(decPin, INPUT);
    }

    void update() {
        (incrementPin, &incrementButtonState, &lastIncrementPressTime, &incrementPressed);
        handleButtonPress(decrementPin, &decrementButtonState, &lastDecrementPressTime, &decrementPressed);
    }

    int getValue() {
        return value;
    }

    void setValue(int new_value) {
      value = new_value;
    }

private:
    const uint8_t incrementPin;
    const uint8_t decrementPin;
    bool incrementButtonState, decrementButtonState;
    bool incrementPressed = false, decrementPressed = false;
    unsigned long lastIncrementPressTime = 0, lastDecrementPressTime = 0;
    int value = 0;

    void handleButtonPress(uint8_t buttonPin, bool *buttonState, unsigned long *lastPressTime, bool *isPressed) {
        *buttonState = !digitalRead(buttonPin);
        unsigned long currentTime = millis();
        if (*buttonState == HIGH) {
            if (!*isPressed) {
                *isPressed = true;
                *lastPressTime = currentTime;
                updateValue(buttonPin == incrementPin);
            } else if (currentTime - *lastPressTime > 1000) {
                updateValue(buttonPin == incrementPin); // Rapid increment/decrement after 1 second
            }
        } else if (*isPressed) {
            *isPressed = false;
        }
    }

    void updateValue(bool increase) {
        if (increase) value++;
        else if (value > 0) value--;
    }
};


LEDMode* currentMode;
RunningDotMode runningDotMode;
LightSwitchMode lightSwitchMode;
GradualFillMode gradualFillMode;
GameMode gameMode;

LEDSettingsManager settings;

bool prev_button_state = false;
bool settings_modified = false;

void setup() {
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    settings = LEDSettingsManager();
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(RED_BTN, INPUT_PULLUP);
    pinMode(GRN_BTN, INPUT_PULLUP);
    pinMode(DIP1, INPUT_PULLUP);
    pinMode(DIP2, INPUT_PULLUP);
    pinMode(DIP3, INPUT_PULLUP);
    pinMode(DIP4, INPUT_PULLUP);
    pinMode(DIP_SET, INPUT_PULLUP);
    Serial.begin(9600);
    currentMode = &runningDotMode; // Default mode
}

BTN_INPUT buttonInput(GRN_BTN, RED_BTN);

void loop() {
  //read dip 4 as setting_mode. If the dip is active, settings can be edited.
  bool settings_mode = !digitalRead(DIP_SET);
  //read dip 1-3 as 3-bit number 0..7
  int dip_number = 0;
  dip_number |= !digitalRead(DIP1) << 0;
  dip_number |= !digitalRead(DIP2) << 1;
  dip_number |= !digitalRead(DIP3) << 2;
  dip_number |= !digitalRead(DIP4) << 3;

  if(settings_mode){
    //SETTINGS MODE
    buttonInput.setValue(settings.get_setting(static_cast<LEDSetting>(dip_number)));
    buttonInput.update();
    bool button_value = buttonInput.getValue();
    // Replace the potentiometer reading with button input
    settings.set_setting(static_cast<LEDSetting>(dip_number), button_value);
    settings_modified = true;
    Serial.println(buttonInput.getValue());
    settings.display_setting(static_cast<LEDSetting>(dip_number), false);
  }
  else  {
    //RUN MODE
    if(settings_modified){
      //if returning from settings, and settings have been modified, write new settings to eeprom
      settings_modified = false;
      settings.write_to_eeprom();
    }
    // FastLED.setBrightness(map(analogRead(POT_PIN), 1023, 0, 5, 255)); // Control brightness with potentiometer
    FastLED.setBrightness(255);
    switch (dip_number) {
        case 0: currentMode = &runningDotMode; break;
        case 1: currentMode = &lightSwitchMode; break;
        case 2: currentMode = &gradualFillMode; break;
        case 3: currentMode = &gameMode; break;
        // Additional cases for other modes
    }
    currentMode->update(settings);    
  }
  FastLED.show();
}
