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
              value = defaultValue;
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

        void reset(){
          // resets the value to default value
          value = defaultValue;
        }
};

class LEDSettingsManager {
    public:
        Setting settings[SETTINGS_COUNT];
        LEDSettingsManager() {
            // Initialize settings with default values
            initialize();
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

        void writeToEEPROM() {
            int address = 0;
            for (int i = 0; i < SETTINGS_COUNT; ++i) {
                EEPROM.put(address, settings[i].value);
                address += sizeof(int); // Move to the next address, considering the size of an int
            }   
            Serial.println("Written to EEPROM!");
        }

        void readFromEEPROM() {
            int address = 0;
            for (int i = 0; i < SETTINGS_COUNT; ++i) {
                EEPROM.get(address, settings[i].value);
                address += sizeof(int); // Move to the next address
            }      
        }

        void initialize(){
            settings[COLOR_RED] = Setting(0, 255, 255, 5, CRGB::Red);
            settings[COLOR_GREEN] = Setting(0, 255, 255, 5, CRGB::Green);
            settings[COLOR_BLUE] = Setting(0, 255, 255, 5, CRGB::Blue);
            settings[SPEED] = Setting(0, 1024, 5, 5, CRGB::Purple);
            settings[NUMBER_LEDS] = Setting(50, 350, 350, 1, CRGB::Purple);
            settings[INDEX1] = Setting(0, 255, 20, 1, CRGB::Yellow);
            settings[COLOR_RED2] = Setting(0, 255, 255, 5, CRGB::Red);
            settings[COLOR_GREEN2] = Setting(0, 255, 255, 5, CRGB::Green);
            settings[COLOR_BLUE2] = Setting(0, 255, 255, 5, CRGB::Blue);
            settings[INDEX2] = Setting(0, 255, 40, 1, CRGB::Yellow);
            settings[COLOR_RED3] = Setting(0, 255, 255, 5, CRGB::Red);
            settings[COLOR_GREEN3] = Setting(0, 255, 255, 5, CRGB::Green);
            settings[COLOR_BLUE3] = Setting(0, 255, 255, 5, CRGB::Blue);
            settings[RANDOM] = Setting(0, 1024, 1, 1, CRGB::White);
            settings[BROKEN_MODE] = Setting(0, 10, 1, 1, CRGB::White);
            settings[BROKEN_THRESHOLD] = Setting(0, 255, 50, 1, CRGB::White);
        }

        void resetToDefault(){
            // init and write new values to eeprom
            initialize();
            writeToEEPROM();
        }

        void debugPrint() {
            for (int i = 0; i < SETTINGS_COUNT; ++i) {
                Serial.print(settings[i].value);
                Serial.print(i < SETTINGS_COUNT - 1 ? ", " : "\n");
            }
        }
};

// RUN MODES

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
            //delay(2);
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
        int ledIndex = 0; // Current LED index
        unsigned long lastUpdate = 0; // Last update time

        CRGB getColorForIndex(LEDSettingsManager &settings, int index) {
            int index1 = settings.getSetting(INDEX1);
            int index2 = settings.getSetting(INDEX2);

            if (index < index1) {
                return CRGB(settings.getSetting(COLOR_RED), settings.getSetting(COLOR_GREEN), settings.getSetting(COLOR_BLUE));
            } else if (index < index2) {
                return CRGB(settings.getSetting(COLOR_RED2), settings.getSetting(COLOR_GREEN2), settings.getSetting(COLOR_BLUE2));
            } else {
                return CRGB(settings.getSetting(COLOR_RED3), settings.getSetting(COLOR_GREEN3), settings.getSetting(COLOR_BLUE3));
            }
        }

    public:
        void update(LEDSettingsManager settings, ControlManager controlManager) override {
            unsigned long currentTime = millis();
            int speed = settings.getSetting(SPEED); // Get speed setting

            if (controlManager.buzzer.state) {
                if (currentTime - lastUpdate > speed && ledIndex < NUM_LEDS) {
                    leds[ledIndex] = getColorForIndex(settings, ledIndex);
                    ledIndex++;
                    lastUpdate = currentTime;
                }
            } else {
                if (currentTime - lastUpdate > speed && ledIndex > 0) {
                    ledIndex--;
                    leds[ledIndex] = CRGB::Black;
                    lastUpdate = currentTime;
                }
            }

            // If buzzer is not pressed and all LEDs are off, reset ledIndex to 0
            if (!controlManager.buzzer.state && ledIndex == 0) {
                fill_solid(leds, NUM_LEDS, CRGB::Black); // Ensure all LEDs are off
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

class LEDCounterMode : public LEDMode {
    private:
        int ledCount = 0; // Number of LEDs turned on

    public:
        void update(LEDSettingsManager settings, ControlManager controlManager) override {
            // Turn on additional LEDs with the green button
            if (controlManager.greenBtn.pressed) {
                ledCount = min(ledCount + 10, NUM_LEDS);
            }

            // Turn off LEDs with the red button
            if (controlManager.redBtn.pressed) {
                ledCount = max(ledCount - 10, 0);
            }

            // Turn on one more LED with the buzzer button
            if (controlManager.buzzer.pressed) {
                ledCount = min(ledCount + 1, NUM_LEDS);
            }

            // Update LED strip based on the current count
            for (int i = 0; i < NUM_LEDS; i++) {
                leds[i] = i < ledCount ? CRGB::White : CRGB::Black;
            }

        }
};

class DebugMode : public LEDMode {
    public:
        void update(LEDSettingsManager settings, ControlManager controlManager) override {
            // Set all LEDs to white
            fill_solid(leds, NUM_LEDS, CRGB::White);
        }
};



LEDMode* currentMode;
RunningDotMode runningDotMode;
LightSwitchMode lightSwitchMode;
GradualFillMode gradualFillMode;
GameMode gameMode;
LEDCounterMode ledCounterMode;
DebugMode debugMode;

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
        case 4: currentMode = &ledCounterMode; break;
        case 15: currentMode = &debugMode; break;
        // Additional cases for other modes
    }
    Serial.println(controlManager.getDipValue());
    currentMode->update(settings, controlManager);    
  }
  FastLED.show();
}
