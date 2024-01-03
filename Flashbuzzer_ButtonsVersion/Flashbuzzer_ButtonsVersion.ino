#include <FastLED.h>
#include <EEPROM.h>

#define LED_PIN         3
#define NUM_LEDS        255
#define LED_TYPE        WS2811
#define COLOR_ORDER     RGB
#define SETTINGS_COUNT  16

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
    INDEX1,
    COLOR_RED2,
    COLOR_GREEN2,
    COLOR_BLUE2,
    INDEX2,
    COLOR_RED3,
    COLOR_GREEN3,
    COLOR_BLUE3,
    RANDOM,
    BROKEN_MODE,
    BROKEN_THRESHOLD
};

class Button {
  public:
    bool state = false;
    bool lastState = false;
    bool pressed = false;
    bool released = false;
    bool holding = false; // Indicates if the button is being held
    int pin = 0;
    bool analogPin = false;
    bool inverted = true;
    unsigned long pressStartTime = 0; // Time when the button was pressed
    unsigned long holdTimeout = 500; // Default hold timeout in milliseconds

    Button() {}

    Button(int pinNumber, unsigned long holdTimeoutMs = 500) {
      pin = pinNumber;
      holdTimeout = holdTimeoutMs;
      analogPin = (pinNumber >= A0 && pinNumber <= A7); // Check if it's an analog pin
      pinMode(pinNumber, INPUT_PULLUP);
    }

    void update() {
      lastState = state;
      bool tmpState = false;
      unsigned long currentTime = millis();

      if (analogPin) {
        tmpState = analogRead(pin) > 400;
      } else {
        tmpState = digitalRead(pin);
      }

      if (inverted) {
        state = !tmpState;
      } else {
        state = tmpState;
      }

      pressed = !lastState && state;
      released = lastState && !state;

      // Handling the hold functionality
      if (pressed) {
        pressStartTime = currentTime; // Record the time when button is pressed
      }

      if (state) {
        if ((currentTime - pressStartTime) >= holdTimeout) {
          holding = true; // Button is considered as being held
        }
      } else {
        holding = false; // Reset holding when button is released
      }
    }
};

class ControlManager {
    public:
        Button buzzer;
        Button redBtn, greenBtn;
        Button dipSwitches[4];
        Button settingsBtn;

    private:
        int incDecValue = 0;
        int incDecMin = 0;
        int incDecMax = 100; // default max value
        int incDecScrollSpeed = 5;
        int incDecSkipSpeed = 1;
        unsigned long incDecThreshold = 1500; // default threshold in ms
        unsigned long lastButtonPressTime = 0;
        bool scrolling = false;

    public:
        ControlManager(int redBtnPin, int greenBtnPin, int settingsBtnPin, int dipPins[4], int buzzerPin) 
            : redBtn(redBtnPin), greenBtn(greenBtnPin), settingsBtn(settingsBtnPin), buzzer(buzzerPin) {
            for (int i = 0; i < 4; i++) {
                dipSwitches[i] = Button(dipPins[i]);
            }
            buzzer.inverted = false;
        }

        void update() {
            redBtn.update();
            greenBtn.update();
            settingsBtn.update();
            buzzer.update();
            for (auto &dipSwitch : dipSwitches) {
                dipSwitch.update();
            }
            handleIncDec();
        }

        LEDSetting getDipValueAsLEDSetting() {
            int value = 0;
            for (int i = 0; i < 4; i++) {
                if (dipSwitches[i].state) {
                    value |= 1 << i;
                }
            }
            return static_cast<LEDSetting>(value);
        }

        LEDSetting getDipValue() {
            int value = 0;
            for (int i = 0; i < 4; i++) {
                if (dipSwitches[i].state) {
                    value |= 1 << i;
                }
            }
            return value;
        }

        void setIncDecValue(int value) {
            incDecValue = value;
        }

        int getIncDecValue() {
            return incDecValue;
        }

        void setIncDecMin(int min) {
            incDecMin = min;
        }

        void setIncDecMax(int max) {
            incDecMax = max;
        }

        void setIncDecScrollSpeed(int speed) {
            incDecScrollSpeed = speed;
        }

        void setIncDecThreshold(unsigned long threshold) {
            incDecThreshold = threshold;
        }

        void setIncDecSkipSpeed(int speed) {
            incDecSkipSpeed = speed;
        }
        void debugPrint() {
            Serial.print("Buzzer=");
            Serial.print(buzzer.state ? "1" : "0");
            Serial.print(" Red=");
            Serial.print(redBtn.state ? "1" : "0");
            Serial.print(", Green=");
            Serial.print(greenBtn.state ? "1" : "0");
            Serial.print(", Settings=");
            Serial.print(settingsBtn.state ? "1" : "0");
            Serial.print(", DIP=");
            for (int i = 0; i < 4; i++) {
                Serial.print(dipSwitches[i].state ? "1" : "0");
            }
            Serial.print(", IncDecValue=");
            Serial.print(incDecValue);
            Serial.println();
        }

    private:
        bool Increment() {

            if (incDecValue < incDecMax) {
                incDecValue += scrolling ? incDecScrollSpeed : incDecSkipSpeed;
                if(incDecValue > incDecMax){
                  incDecValue = incDecMax;
                }
                return true;
            } else {
              return false;
            }
        }

        bool Decrement() {
            if (incDecValue > incDecMin) {
                incDecValue -= scrolling ? incDecScrollSpeed : incDecSkipSpeed;
                if(incDecValue < incDecMin){
                  incDecValue = incDecMin;
                }
                return true;
            } else {
              return false;
            }
        }

        void handleIncDec() {
            scrolling = (millis() - lastButtonPressTime) >= incDecThreshold && (redBtn.state && !redBtn.pressed || greenBtn.state && !greenBtn.pressed);
            if (redBtn.pressed) {
                lastButtonPressTime = millis();
                Increment();
            } else if (greenBtn.pressed) {
                lastButtonPressTime = millis();
                Decrement();
            } else if (scrolling) {
                if (redBtn.state) {
                    Increment();
                } else if (greenBtn.state) {
                    Decrement();
                }
            }
        }
};

class Setting {
    public:
        int value;
        int minimum;
        int maximum;
        int increment;
        int defaultValue;
        CRGB color; // Color used for displaying the setting

        Setting(){}

        Setting(int minValue, int maxValue, int defaultValue, int stdIncrement, CRGB settingColor)
            : minimum(minValue), maximum(maxValue), value(defaultValue), increment(stdIncrement), color(settingColor) {
              defaultValue = value;
            }

        void increase() {
            value = min(value + increment, maximum);
        }

        void decrease() {
            value = max(value - increment, minimum);
        }

        void draw(CRGB leds[]) {
            int numLedsToLight = map(value, 0, maximum, 0, NUM_LEDS);

            for (int i = 0; i < NUM_LEDS; ++i) {
                leds[i] = (i < numLedsToLight) ? color : CRGB::Black;
            }
            // Note: FastLED.show() should be called externally after this function
        }
};

class LEDSettingsManager {
    public:
        Setting settings[SETTINGS_COUNT];
        LEDSettingsManager() {
            // Initialize settings with default values
            settings[COLOR_RED] = Setting(0, 255, 255, 5, CRGB::Red);
            settings[COLOR_GREEN] = Setting(0, 255, 255, 5, CRGB::Green);
            settings[COLOR_BLUE] = Setting(0, 255, 255, 5, CRGB::Blue);
            settings[SPEED] = Setting(0, 1024, 5, 5, CRGB::Purple);
            settings[NUMBER_LEDS] = Setting(50, 350, 350, 1, CRGB::Purple);
            settings[INDEX1] = Setting(0, 255, 20, 1, CRGB::Yellow); // Example initializer
            settings[COLOR_RED2] = Setting(0, 255, 255, 5, CRGB::Red);
            settings[COLOR_GREEN2] = Setting(0, 255, 255, 5, CRGB::Green);
            settings[COLOR_BLUE2] = Setting(0, 255, 255, 5, CRGB::Blue);
            settings[INDEX2] = Setting(0, 255, 40, 1, CRGB::Yellow); // Example initializer
            settings[COLOR_RED3] = Setting(0, 255, 255, 5, CRGB::Red);
            settings[COLOR_GREEN3] = Setting(0, 255, 255, 5, CRGB::Green);
            settings[COLOR_BLUE3] = Setting(0, 255, 255, 5, CRGB::Blue);
            settings[RANDOM] = Setting(0, 1024, 1, 1, CRGB::White); // Example initializer
            settings[BROKEN_MODE] = Setting(0, 10, 1, 1, CRGB::White); // Example initializer
            settings[BROKEN_THRESHOLD] = Setting(0, 255, 50, 1, CRGB::White); // Example initializer

            readFromEEPROM();
        }

        void setSetting(LEDSetting setting, int value) {
            settings[setting].value = constrain(value, settings[setting].minimum, settings[setting].maximum);
        }

        int getSetting(LEDSetting setting) {
            return settings[setting].value;
        }

        void incrementSetting(LEDSetting setting) {
            settings[setting].increase();
        }

        void decrementSetting(LEDSetting setting) {
            settings[setting].decrease();
        }

        void drawSetting(CRGB leds[], LEDSetting setting) {
            settings[setting].draw(leds);
            // Call FastLED.show() after this function to update the LED strip
        }

        bool isDark(){
            return settings[COLOR_RED].value == 0 && settings[COLOR_GREEN].value == 0 && settings[COLOR_BLUE].value == 0;
        }

        CRGB getColor(){
            return CRGB(settings[COLOR_RED].value, settings[COLOR_GREEN].value, settings[COLOR_BLUE].value);
        }

        void writeToEEPROM(){
          //write values to EEPROM
          for (int i = 0; i < SETTINGS_COUNT; ++i) {
              EEPROM.write(i, settings[i].value);
          }   
          Serial.println("Written to EEPROM!");
        }

        void readFromEEPROM(){
            // Read settings from EEPROM and overwrite defaults if necessary
            for (int i = 0; i < SETTINGS_COUNT; ++i) {
                int eepromValue = EEPROM.read(i);
                settings[i].value = eepromValue;
            }      
        }

        void resetToDefault(){
          for (int i = 0; i < SETTINGS_COUNT; i++){
            settings[i].value = settings[i].defaultValue;
          }
        }

        void debugPrint() {
            for (int i = 0; i < SETTINGS_COUNT; ++i) {
                Serial.print(settings[i].value);
                Serial.print(i < SETTINGS_COUNT - 1 ? ", " : "\n");
            }
        }
};


class LEDMode {
    public:
        virtual void update(LEDSettingsManager settings, ControlManager controlManager) = 0;
};

class RunningDotMode : public LEDMode {
    private:

    public:
        void update(LEDSettingsManager settings, ControlManager controlManager) override {
            bool newButtonState = digitalRead(BUTTON_PIN);
            if (controlManager.buzzer.pressed) {
                CRGB color = settings.isDark() ? CRGB(random(255), random(255), random(255)) : settings.getColor(); // Random or white color if no color selected
                leds[0] = color;
            }

            for (int i = NUM_LEDS - 1; i > 0; i--) {
                leds[i] = leds[i - 1];
            }
            leds[0] = CRGB::Black;

            // delay(map(settings.settings[SPEED].value, 0, 255, 300, 3)); // Speed of shifting
            delay(2);
        }
};

class LightSwitchMode : public LEDMode {
    private:
        bool isOn = false;
        bool oldButtonState = HIGH;

    public:
        void update(LEDSettingsManager settings, ControlManager controlManager) override {
            bool newButtonState = !digitalRead(BUTTON_PIN);
            if (newButtonState == LOW && oldButtonState == HIGH) {
                isOn = !isOn;
                CRGB color = isOn ? (settings.isDark() ? CRGB(random(255), 255, 255) : settings.getColor()) : CRGB::Black; // Random or white color based on parameter
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
        void update(LEDSettingsManager settings, ControlManager controlManager) override {
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
        void update(LEDSettingsManager settings, ControlManager controlManager) override {
            // Placeholder for game mode
            // Implement game logic here
        }
};


LEDMode* currentMode;
RunningDotMode runningDotMode;
LightSwitchMode lightSwitchMode;
GradualFillMode gradualFillMode;
GameMode gameMode;

LEDSettingsManager settings = LEDSettingsManager();
bool settings_modified = false;

int dipPins[4] = {DIP1, DIP2, DIP3, DIP4};
ControlManager controlManager(RED_BTN, GRN_BTN, DIP_SET, dipPins, BUTTON_PIN);

void setup() {
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    static LEDSettingsManager settings = LEDSettingsManager();
    // settings.resetToDefault(); // use this if EEPROM is corrupted
    Serial.begin(31250);
    currentMode = &runningDotMode; // Default mode
}

void loop() {
  //read dip 4 as setting_mode. If the dip is active, settings can be edited.
  controlManager.update(); //read all inputs
  // controlManager.debugPrint();
  if(controlManager.settingsBtn.state){
    //SETTINGS MODE
    LEDSetting currSetting = controlManager.getDipValueAsLEDSetting();
    settings.drawSetting(leds, currSetting);
    if(controlManager.greenBtn.pressed || controlManager.greenBtn.holding){
      settings.incrementSetting(currSetting);
      settings_modified = true;
    } 
    if(controlManager.redBtn.pressed || controlManager.redBtn.holding){
      settings.decrementSetting(currSetting);
      settings_modified = true;
    } 
    // settings.debugPrint();
    // controlManager.debugPrint();
    delay(5);
  }
  else  {
    //RUN MODE
    if(settings_modified){
      //if returning from settings, and settings have been modified, write new settings to eeprom
      settings_modified = false;
      settings.writeToEEPROM();
    }
    // FastLED.setBrightness(map(analogRead(POT_PIN), 1023, 0, 5, 255)); // Control brightness with potentiometer
    FastLED.setBrightness(255);
    switch (controlManager.getDipValue()) {
        case 0: currentMode = &runningDotMode; break;
        case 1: currentMode = &lightSwitchMode; break;
        case 2: currentMode = &gradualFillMode; break;
        case 3: currentMode = &gameMode; break;
        // Additional cases for other modes
    }
    currentMode->update(settings, controlManager);    
  }
  FastLED.show();
}
