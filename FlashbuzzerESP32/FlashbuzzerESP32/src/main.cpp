#include <map>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Arduino.h>
#include <Preferences.h>  // For non-volatile memory storage (NVS)
#include <FastLED.h>

#define LED_PIN 16          // Pin to which the LED strip is connected
#define NUM_LEDS 300        // Number of LEDs in the strip
#define BRIGHTNESS 200      // Brightness of the LEDs (0-255)
#define COLOR CRGB::Red     // Color of the running dot
#define BUTTON_PIN 13  // Pin where the button is connected

class RunningDot {
public:
    // Constructor: Initializes the LED strip
    RunningDot() : currentPosition(0) {
        FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
        FastLED.setBrightness(BRIGHTNESS);
        FastLED.clear();
        FastLED.show();
    }

    // This function moves the dot one step forward on the strip
    void trigger() {
        // Clear previous position
        FastLED.clear();
        
        // Set the new position for the running dot
        leds[currentPosition] = COLOR;
        
        // Move to the next position, wrap around if needed
        currentPosition = (currentPosition + 1) % NUM_LEDS;
        
        // Show the update
        FastLED.show();
    }

private:
    CRGB leds[NUM_LEDS];    // Array to store the LED colors
    int currentPosition;    // Current position of the running dot
};

class ConfigParameter {
public:
    enum Type { STRING, FLOAT };

    // Default constructor
    ConfigParameter() : type(FLOAT), floatValue(0.0f) {}

    // Constructor for String parameter
    ConfigParameter(const String& name, const String& value)
        : name(name), type(STRING) {
        stringValue = new String(value);
    }

    // Constructor for Float parameter
    ConfigParameter(const String& name, float value)
        : name(name), type(FLOAT), floatValue(value) {}

    // Copy constructor
    ConfigParameter(const ConfigParameter& other)
        : name(other.name), type(other.type) {
        if (type == STRING) {
            stringValue = new String(*other.stringValue);
        } else {
            floatValue = other.floatValue;
        }
    }

    // Move constructor
    ConfigParameter(ConfigParameter&& other) noexcept
        : name(std::move(other.name)), type(other.type) {
        if (type == STRING) {
            stringValue = other.stringValue;
            other.stringValue = nullptr;
        } else {
            floatValue = other.floatValue;
        }
    }

    // Destructor to properly handle the string memory
    ~ConfigParameter() {
        if (type == STRING && stringValue) {
            delete stringValue;
        }
    }

    // Copy assignment operator
    ConfigParameter& operator=(const ConfigParameter& other) {
        if (this == &other) return *this;
        name = other.name;
        type = other.type;
        if (type == STRING) {
            if (stringValue) {
                *stringValue = *other.stringValue;
            } else {
                stringValue = new String(*other.stringValue);
            }
        } else {
            floatValue = other.floatValue;
        }
        return *this;
    }

    // Move assignment operator
    ConfigParameter& operator=(ConfigParameter&& other) noexcept {
        if (this == &other) return *this;
        name = std::move(other.name);
        type = other.type;
        if (type == STRING) {
            delete stringValue;
            stringValue = other.stringValue;
            other.stringValue = nullptr;
        } else {
            floatValue = other.floatValue;
        }
        return *this;
    }

    // Getter for parameter name
    String getName() const { return name; }

    // Getter for type
    Type getType() const { return type; }

    // Get the String value (only call if type is STRING)
    String getStringValue() const {
        if (type == STRING && stringValue) return *stringValue;
        return "";
    }

    // Get the float value (only call if type is FLOAT)
    float getFloatValue() const {
        if (type == FLOAT) return floatValue;
        return 0.0;
    }

    // Setter for String value
    void setValue(const String& newValue) {
        if (type == STRING) {
            if (stringValue) {
                *stringValue = newValue;
            } else {
                stringValue = new String(newValue);
            }
        }
    }

    // Setter for float value
    void setValue(float newValue) {
        if (type == FLOAT) {
            floatValue = newValue;
        }
    }

private:
    String name;
    Type type;
    union {
        String* stringValue;
        float floatValue;
    };
};

class WebConfig {
public:
    WebConfig(const char* ssid, const char* password)
        : softAP_ssid(ssid), softAP_password(password), server(80), title("Configuration Page") {}

    void begin() {
        preferences.begin("webconfig", false);  // Open NVS with namespace 'webconfig'
        loadParameters();  // Load parameters from NVS on startup
        configureAccessPoint();
        setupDNS();
        setupWebServer();
    }

    void handleClient() {
        dnsServer.processNextRequest();
        server.handleClient();
    }

    void addParamString(const String& name, const String& defaultValue) {
        configParams[name] = ConfigParameter(name, defaultValue);
        saveParameter(name);  // Save new or modified parameter to NVS
    }

    void addParamFloat(const String& name, float defaultValue) {
        configParams[name] = ConfigParameter(name, defaultValue);
        saveParameter(name);  // Save new or modified parameter to NVS
    }

    String getParamString(const String& name) {
        return configParams[name].getStringValue();
    }

    float getParamFloat(const String& name) {
        return configParams[name].getFloatValue();
    }

    void setParam(const String& name, const String& value) {
        if (configParams[name].getType() == ConfigParameter::STRING) {
            configParams[name].setValue(value);
            saveParameter(name);  // Save modified parameter to NVS
        }
    }

    void setParam(const String& name, float value) {
        if (configParams[name].getType() == ConfigParameter::FLOAT) {
            configParams[name].setValue(value);
            saveParameter(name);  // Save modified parameter to NVS
        }
    }

    // New method to set the dynamic title
    void setTitle(const String& newTitle) {
        title = newTitle;
    }

    // Uncomment this method to clear all stored parameters and reset
    /*
    void resetParameters() {
        preferences.clear();  // Clear all stored preferences
    }
    */

private:
    const char* softAP_ssid;
    const char* softAP_password;
    IPAddress apIP = IPAddress(8, 8, 8, 8); // Access Point IP Address
    IPAddress netMsk = IPAddress(255, 255, 255, 0); // Netmask
    const byte DNS_PORT = 53;
    DNSServer dnsServer;
    WebServer server;
    String title;  // Dynamic title for the configuration page

    Preferences preferences;  // NVS Preferences for storing parameters

    std::map<String, ConfigParameter> configParams; // Configuration storage

    void configureAccessPoint() {
        WiFi.softAPConfig(apIP, apIP, netMsk);
        WiFi.softAP(softAP_ssid, softAP_password);
        delay(1000);
        Serial.print("AP IP address: ");
        Serial.println(WiFi.softAPIP());
    }

    void setupDNS() {
        dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
        dnsServer.start(DNS_PORT, "*", apIP);
    }

    void setupWebServer() {
        server.on("/", [this]() { handleRoot(); });
        server.on("/generate_204", [this]() { handleRoot(); }); // Handle Android captive portal request
        server.on("/submit", [this]() { handleSubmit(); }); // Form submission
        server.onNotFound([this]() { handleNotFound(); });
        server.begin();
        Serial.println("HTTP server started");
    }

    // Save parameter to NVS
    void saveParameter(const String& paramName) {
        ConfigParameter& param = configParams[paramName];
        if (param.getType() == ConfigParameter::STRING) {
            preferences.putString(paramName.c_str(), param.getStringValue());
        } else if (param.getType() == ConfigParameter::FLOAT) {
            preferences.putFloat(paramName.c_str(), param.getFloatValue());
        }
    }

    // Load all parameters from NVS
    void loadParameters() {
        for (const auto& param : configParams) {
            String paramName = param.first;
            if (param.second.getType() == ConfigParameter::STRING) {
                String value = preferences.getString(paramName.c_str(), "");
                if (value != "") {
                    configParams[paramName].setValue(value);
                }
            } else if (param.second.getType() == ConfigParameter::FLOAT) {
                float value = preferences.getFloat(paramName.c_str(), 0.0f);
                configParams[paramName].setValue(value);
            }
        }
    }

    void handleRoot() {
        if (captivePortal()) {
            return;
        }
        server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        server.sendHeader("Pragma", "no-cache");
        server.sendHeader("Expires", "-1");

        // Create an HTML page with a dynamic title and tabs for each group
        String p = F("<html><head>"
                    "<style>"
                    "body {"
                    "  margin: 0;"
                    "  font-family: Arial, sans-serif;"
                    "  background-color: #f0f0f0;"
                    "  height: 100vh;"
                    "  overflow-x: hidden;" /* Prevent horizontal scroll */
                    "}"
                    ".header {"
                    "  width: 100%;"
                    "  background-color: #fff;"
                    "  padding: 10px 0;"
                    "  position: sticky;" /* Keep the header at the top when scrolling */ 
                    "  top: 0;"
                    "  z-index: 1000;"
                    "  box-shadow: 0 2px 4px rgba(0,0,0,0.1);"
                    "  text-align: center;"
                    "}"
                    ".header h1 {"
                    "  font-size: 5em;"
                    "  color: #333;"
                    "  margin: 10px 0;"
                    "  -webkit-text-stroke: 3px transparent;"
                    "  text-shadow: 0 0 12px rgba(0, 0, 0, 0.5);"
                    "  animation: textOutlineAnimation 3s infinite ease-in-out;"
                    "}"
                    "@keyframes textOutlineAnimation {"
                    "  0%, 100% { -webkit-text-stroke: 2px transparent; text-shadow: 0 0 6px rgba(0, 0, 0, 0.5); }"
                    "  50% { -webkit-text-stroke: 2px #4CAF50; text-shadow: none; }"
                    "}"
                    ".svg-container {"
                    "  width: 100%;"
                    "  display: flex;"
                    "  justify-content: center;"
                    "  margin-bottom: 10px;"
                    "  padding: 15;"
                    "}"
                    "svg {"
                    "  width: 60%;"
                    "  max-width: 1080px;"
                    "}"
                    ".svg-outline {"
                    "  fill: none;"
                    "  stroke: black;"
                    "  stroke-width: 2;"
                    "  stroke-dasharray: 10, 5;"
                    "  animation: dash 5s linear infinite;"
                    "}"
                    "@keyframes dash {"
                    "  to { stroke-dashoffset: -50; }"
                    "}"
                    ".tab-container {"
                    "  width: 100%;"
                    "  display: flex;"
                    "  justify-content: center;"
                    "  margin-top: 20px;"
                    "}"
                    "ul {"
                    "  list-style-type: none;"
                    "  padding: 0;"
                    "  margin: 0;"
                    "  width: 80%;" /* Full width of the tab container */
                    "  display: flex;"
                    "  justify-content: center;" /* Center the tabs */
                    "  overflow-x: auto;" /* Allow horizontal scrolling for smaller screens */ 
                    "}"
                    "li {"
                    "  flex: 1;"
                    "  text-align: center;"
                    "  margin-right: 10px;"
                    "}"
                    "a {"
                    "  font-size: 2em;"
                    "  text-decoration: none;"
                    "  color: #333;"
                    "  padding: 10px;"
                    "  background-color: #f0f0f0;"
                    "  border: 1px solid #ccc;"
                    "  border-radius: 5px;"
                    "  display: block;"
                    "  width: 100%;"
                    "  box-sizing: border-box;"
                    "}"
                    "a:hover {"
                    "  background-color: #ddd;"
                    "}"
                    ".tab-content {"
                    "  display: none;"
                    "  width: 80%;"
                    "  padding: 0px;"
                    "  margin: 20px auto;"
                    "}"
                    ".active-tab {"
                    "  display: block;"
                    "}"
                    "form {"
                    "  background: white;"
                    "  padding: 20px;"
                    "  border-radius: 10px;"
                    "  box-shadow: 0 4px 8px rgba(0,0,0,0.1);"
                    "  width: 100%;"
                    "  box-sizing: border-box;"
                    "  margin: 0 auto;"
                    "}"
                    "label, input {"
                    "  display: block;"
                    "  width: 100%;"
                    "  margin-bottom: 3px;"
                    "  font-size: 3em;"
                    "  font-weight: bold;"
                    "}"
                    "input {"
                    "  padding: 10px;"
                    "  border: 1px solid #ccc;"
                    "  border-radius: 5px;"
                    "  font-size: 3em;"
                    "  box-sizing: border-box;"
                    "}"
                    "input[type='submit'] {"
                    "  background-color: #333333;"
                    "  color: white;"
                    "  border: none;"
                    "  cursor: pointer;"
                    "  padding: 15px;"
                    "  transition: background-color 0.3s ease;"
                    "  font-size: 3em;"
                    "}"
                    "input[type='submit']:hover {"
                    "  background-color: #45a049;"
                    "}"
                    "</style>"
                    
                    // JavaScript to handle the tab switching
                    "<script>"
                    "function openTab(tabName) {"
                    "  var i, tabcontent;"
                    "  tabcontent = document.getElementsByClassName('tab-content');"
                    "  for (i = 0; i < tabcontent.length; i++) {"
                    "    tabcontent[i].style.display = 'none';"
                    "  }"
                    "  document.getElementById(tabName).style.display = 'block';"
                    "}"
                    "</script>"
                    
                    "</head><body>");

        // Insert SVG animation and title in a fixed header container
        p += "<div class='header'><div class='svg-container'>";
        p += "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 150 126\">";
        p += "<g transform=\"translate(-28.34617, -67.34671)\">";
        p += "<path class=\"svg-outline\" d=\"M46.648479 131.26477v13.51339h-0.003v17.82217H159.91391V144.77816H64.562629V131.26477ZM126.77385 99.98706h18.61959v18.63263h-18.61959zm-62.580031 0h18.61959v18.63263H64.193819ZM28.346749 67.346711c-0.002 41.819719 0.002 84.474009 0 126.000059h0.0486 149.900931V72.846631h0.0501l-0.0501 -5.49992zm5.49992 5.49992H172.79633V187.84685H33.846669Z\" />";
        p += "</g></svg></div>";
        p += "<h1>" + title + "</h1></div>";

        // Collect group names and parameters
        std::map<String, std::vector<String>> groupedParams;
        std::vector<String> noGroupParams;

        // Organize parameters by group or no group
        for (const auto& param : configParams) {
            String paramName = param.first;
            int underscoreIndex = paramName.indexOf('_');
            if (underscoreIndex != -1) {
                // Grouped parameter (group_name_parameter_name)
                String groupName = paramName.substring(0, underscoreIndex);
                String subParamName = paramName.substring(underscoreIndex + 1);
                groupedParams[groupName].push_back(subParamName);
            } else {
                // Parameter without a group
                noGroupParams.push_back(paramName);
            }
        }

        // Display the tabs for each group and the Home tab for non-grouped parameters
        p += "<div class='tab-container'><ul>";
        p += "<li><a onclick=\"openTab('home')\">Home</a></li>";
        for (const auto& group : groupedParams) {
            p += "<li><a onclick=\"openTab('" + group.first + "')\">" + group.first + "</a></li>";
        }
        p += "</ul></div>";

        // Display non-grouped parameters (Home Tab)
        p += "<div id='home' class='tab-content active-tab'><form action=\"/submit\" method=\"POST\">";
        if (!noGroupParams.empty()) {
            for (const String& paramName : noGroupParams) {
                const ConfigParameter& param = configParams[paramName];
                p += "<label for='" + paramName + "'>" + paramName + ":</label>";
                if (param.getType() == ConfigParameter::STRING) {
                    p += "<input type='text' name='" + paramName + "' value='" + param.getStringValue() + "'><br>";
                } else if (param.getType() == ConfigParameter::FLOAT) {
                    p += "<input type='number' step='any' name='" + paramName + "' value='" + String(param.getFloatValue()) + "'><br>";
                }
            }
            p += "<input type='submit' value='Submit'>";
        } else {
            p += "<p>No parameters available on this page.</p>";
        }
        p += "</form></div>";

        // Display grouped parameters (Each group in its own tab)
        for (const auto& group : groupedParams) {
            p += "<div id='" + group.first + "' class='tab-content'><form action=\"/submit\" method=\"POST\">";
            for (const String& subParamName : group.second) {
                String fullParamName = group.first + "_" + subParamName;
                const ConfigParameter& param = configParams[fullParamName];
                p += "<label for='" + fullParamName + "'>" + subParamName + ":</label>";
                if (param.getType() == ConfigParameter::STRING) {
                    p += "<input type='text' name='" + fullParamName + "' value='" + param.getStringValue() + "'><br>";
                } else if (param.getType() == ConfigParameter::FLOAT) {
                    p += "<input type='number' step='any' name='" + fullParamName + "' value='" + String(param.getFloatValue()) + "'><br>";
                }
            }
            p += "<input type='submit' value='Submit'></form></div>";
        }

        p += "</body></html>";

        server.send(200, "text/html", p);
    }


    void handleSubmit() {
        for (const auto& param : configParams) {
            if (server.hasArg(param.first)) {
                if (param.second.getType() == ConfigParameter::STRING) {
                    setParam(param.first, server.arg(param.first));
                } else if (param.second.getType() == ConfigParameter::FLOAT) {
                    setParam(param.first, server.arg(param.first).toFloat());
                }
            }
        }
        
        // Instead of showing a separate page, reload the current page after submission
        server.send(200, "text/html", "<html><body><script>window.location.href = '/';</script></body></html>");
    }


    void handleNotFound() {
        if (captivePortal()) {
            return;
        }
        String message = "404 Not Found\n\n";
        message += "URI: ";
        message += server.uri();
        message += "\nMethod: ";
        message += (server.method() == HTTP_GET) ? "GET" : "POST";
        message += "\nArguments: ";
        message += server.args();
        message += "\n";
        for (uint8_t i = 0; i < server.args(); i++) {
            message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
        }
        server.send(404, "text/plain", message);
    }

    boolean captivePortal() {
        if (!isIp(server.hostHeader())) {
            Serial.println("Request redirected to captive portal");
            server.sendHeader("Location", String("http://") + toStringIp(server.client().localIP()), true);
            server.send(302, "text/plain", "");
            server.client().stop();
            return true;
        }
        return false;
    }

    bool isIp(String str) {
        for (size_t i = 0; i < str.length(); i++) {
            int c = str.charAt(i);
            if ((c != '.') && (c < '0' || c > '9')) {
                return false;
            }
        }
        return true;
    }

    String toStringIp(IPAddress ip) {
        return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
    }
};

// Global WebConfig object
WebConfig webConfig("esp32_bob", "12345678");
// RunningDot runningDot;

bool laststate = false;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Configuring access point...");
    
    // Set dynamic title for the configuration page
    webConfig.setTitle("ESP32 Device Configuration");

    // Add configuration parameters
    webConfig.addParamFloat("Red", 255.0);
    webConfig.addParamFloat("Green", 255.0);
    webConfig.addParamFloat("Blue", 255.0);
    
    webConfig.begin(); // Start the AP and web server
    // pinMode(BUTTON_PIN, INPUT_PULLUP);

}


void loop() {
    webConfig.handleClient(); // Handle client requests
    // Check if the button is pressed (LOW means pressed, due to internal pull-up)
    // bool state = digitalRead(BUTTON_PIN);
    // Serial.println(state);
    // if (!state && laststate) {
    //     // Trigger the running dot when the button is pressed
    //     runningDot.trigger();
    //     Serial.println("Pressed");
    //     laststate = state;
    //     // Debounce delay to avoid multiple triggers from a single press
    // }
}
