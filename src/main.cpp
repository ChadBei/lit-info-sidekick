#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <U8g2lib.h>
#include <ezButton.h>
#include <time.h>
#include <esp_wifi.h>
#include <ESP32Encoder.h>

// ------------------------------------------------------------------
// CONFIGURATION
// ------------------------------------------------------------------

// OLED Pins (SPI Interface)
#define PIN_CLK GPIO_NUM_18
#define PIN_MOSI GPIO_NUM_19
#define PIN_DC GPIO_NUM_22
#define PIN_CS GPIO_NUM_23

// Button Pins
#define BUTTON_PIN GPIO_NUM_5
#define ROTARY_DATA_PIN1 GPIO_NUM_13
#define ROTARY_DATA_PIN2 GPIO_NUM_27

// Target URL
const char* serverUrl = "https://bible.usccb.org/bible/readings/"; // Today's readings initial format
// const char* serverUrl = "https://bible.usccb.org/bible/readings/012526.cfm";

bool buttonIsPressed = false;
int override = 0; 



// ------------------------------------------------------------------
// DATA STRUCTURES
// ------------------------------------------------------------------
struct Reading {
  String name;    // e.g. "Gospel"
  String address; // e.g. "Mk 4:1-20"
};

Reading dailyReadings[10]; // Store up to 10 readings
int readingCount = 0;     // Total found
int currentIndex = 0;     // Current one on screen
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600;    // Offset in seconds (e.g., -6 hours * 3600 = -18000 for CST)
const int   daylightOffset_sec = 3600; // 3600 seconds = 1 hour (set to 0 if not in DST)
struct tm timeinfo;
String formattedTime = "";
String formattedTimeNum = "";
long lastKnobPosition = 0;
bool showMenu = false;
short menuIndex = 0;

// ------------------------------------------------------------------
// Oled Display Setup
// ------------------------------------------------------------------
U8G2_SH1106_128X64_NONAME_F_4W_SW_SPI u8g2(U8G2_R2, PIN_CLK, PIN_MOSI, PIN_CS, PIN_DC, -1);

// Rotary Encoder
ESP32Encoder knob;

// ------------------------------------------------------------------
// Icons
// ------------------------------------------------------------------
static const unsigned char image_folder_explorer_bits[] = {0x3c,0x00,0x00,0x42,0x00,0x00,0x81,0xff,0x00,0x01,0x00,0x01,0xfd,0x7f,0x01,0x03,0x80,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0x01,0x00,0x01,0xfe,0xff,0x00};
static const unsigned char image_bible_icon_svg_bits[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0e,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0x1f,0x00,0x00,0x00,0x00,0x00,0x00,0xf0,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0x7f,0x00,0x00,0x00,0x00,0x00,0xe0,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0xfc,0xff,0xff,0x01,0x00,0x00,0x00,0x80,0xff,0xff,0xff,0x03,0x00,0x00,0x00,0xf0,0xff,0xff,0xff,0x07,0x00,0x00,0x00,0xfc,0xff,0xff,0xff,0x0f,0x00,0x00,0x80,0xff,0xff,0xff,0xff,0x1f,0x00,0x00,0xf0,0xff,0xff,0xfb,0xff,0x3f,0x00,0x00,0xfc,0xff,0xff,0xf8,0xff,0x7f,0x00,0x00,0xfc,0xff,0xff,0xf0,0xff,0xff,0x00,0x00,0xfe,0xff,0xff,0xe1,0xff,0xff,0x00,0x00,0xfe,0xff,0xff,0xc3,0xe3,0xff,0x01,0x00,0x3e,0xfe,0xff,0x07,0xf0,0xff,0x03,0x00,0x1e,0xff,0xff,0x07,0xfe,0xff,0x07,0x00,0xdc,0xff,0xff,0x03,0xff,0xff,0x0f,0x00,0xc8,0xff,0x7f,0x10,0xfe,0xff,0x1f,0x00,0xe8,0xff,0x7f,0x3e,0xfc,0xff,0x3f,0x00,0xe0,0xff,0xff,0x7f,0xf8,0xff,0x7f,0x00,0xe0,0xff,0xff,0x7f,0xf8,0xff,0xff,0x00,0xe0,0xff,0xff,0xff,0xf0,0xff,0xff,0x01,0xc0,0xff,0xff,0xff,0xe1,0xff,0xff,0x03,0xc0,0xff,0xff,0xff,0xc3,0xff,0xff,0x07,0x80,0xff,0xff,0xff,0x83,0xff,0xff,0x0f,0x00,0xff,0xff,0xff,0x07,0xff,0xff,0x1f,0x00,0xfe,0xff,0xff,0x0f,0xff,0xff,0x3f,0x00,0xfe,0xff,0xff,0xff,0xff,0xff,0x3f,0x00,0xfc,0xff,0xff,0xff,0xff,0xff,0x17,0x00,0xfc,0xff,0xff,0xff,0xff,0xff,0x18,0x00,0xf8,0xff,0xff,0xff,0xff,0x3f,0x0c,0x00,0xf0,0xff,0xff,0xff,0xff,0x07,0x0c,0x00,0xf0,0xff,0xff,0xff,0xff,0x01,0x0c,0x00,0xe0,0xdf,0xff,0xff,0x1f,0x00,0x0c,0x00,0xc0,0xc7,0xff,0xff,0x07,0x00,0x1c,0x00,0xc0,0xe3,0xff,0xff,0x00,0x00,0x38,0x00,0x80,0xf9,0xff,0x3f,0x00,0x00,0x78,0x00,0x00,0xf9,0xff,0x07,0x00,0x00,0xfc,0x00,0x00,0xfd,0xff,0x00,0x00,0x80,0x3f,0x00,0x00,0xfc,0x3f,0x00,0x00,0xf0,0x07,0x00,0x00,0xfc,0x07,0x00,0x00,0xfe,0x00,0x00,0x00,0xfc,0x01,0x00,0x80,0x1f,0x00,0x00,0x00,0xf8,0x00,0x00,0xf8,0x03,0x00,0x00,0x00,0xf8,0xc0,0x01,0x7f,0x00,0x00,0x00,0x00,0xf0,0xf0,0xe3,0x0f,0x00,0x00,0x00,0x00,0xe0,0xe0,0xff,0x01,0x00,0x00,0x00,0x00,0xe0,0xc0,0x7f,0x00,0x00,0x00,0x00,0x00,0xc0,0xe1,0x1f,0x00,0x00,0x00,0x00,0x00,0x80,0xff,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,0x1f,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const unsigned char image_cloud_bits[] = {0x00,0x00,0x00,0xe0,0x03,0x00,0x10,0x04,0x00,0x08,0x08,0x00,0x0c,0x10,0x00,0x02,0x70,0x00,0x01,0x80,0x00,0x01,0x00,0x01,0x02,0x00,0x01,0xfc,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const unsigned char image_cloud_download_bits[] = {0x00,0x00,0x00,0xe0,0x03,0x00,0x10,0x04,0x00,0x08,0x08,0x00,0x0c,0x10,0x00,0x02,0x70,0x00,0x01,0x80,0x00,0x01,0x00,0x01,0x02,0x01,0x01,0x3c,0xf9,0x00,0x00,0x01,0x00,0x00,0x01,0x00,0xc0,0x07,0x00,0x80,0x03,0x00,0x00,0x01,0x00,0x00,0x00,0x00};
static const unsigned char image_wifi_bits[] = {0x80,0x0f,0x00,0x60,0x30,0x00,0x18,0xc0,0x00,0x84,0x0f,0x01,0x62,0x30,0x02,0x11,0x40,0x04,0x08,0x87,0x00,0xc4,0x18,0x01,0x20,0x20,0x00,0x10,0x42,0x00,0x80,0x0d,0x00,0x40,0x10,0x00,0x00,0x02,0x00,0x00,0x05,0x00,0x00,0x02,0x00,0x00,0x00,0x00};
static const unsigned char image_phone_book_open_bits[] = {0x07,0xc0,0x01,0x1d,0x70,0x01,0x71,0x1c,0x01,0xc5,0x46,0x01,0x19,0x31,0x01,0x61,0x0d,0x01,0x0d,0x61,0x01,0x31,0x19,0x01,0x45,0x45,0x01,0x19,0x31,0x01,0x61,0x0d,0x01,0x07,0xc1,0x01,0x1e,0xf1,0x00,0xf8,0x3f,0x00,0xe0,0x0e,0x00,0x80,0x03,0x00};


// Setup Button
ezButton button(BUTTON_PIN);

// ------------------------------------------------------------------
// FORWARD DECLARATIONS
// ------------------------------------------------------------------
void setupNetwork();
void printDisplay_Connecting(String status, String step, String wifiName, bool showWifiIcon);
void printDisplay(String line1, String line2, String line3);
void updateScreen();
void fetchAllReadings();
void drawSplash();
String getUserHardwareInput(String prompt);
String scanAndSelectWiFi();

// ------------------------------------------------------------------
// SETUP
// ------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  while(!Serial) { delay(10); } // Wait for serial monitor
  
  // Init Hardware
  u8g2.begin(); // Screen Init
  button.setDebounceTime(50); // 50ms debounce

  // New Encoder Setup
  ESP32Encoder::useInternalWeakPullResistors = puType::up;
  knob.attachHalfQuad(ROTARY_DATA_PIN1, ROTARY_DATA_PIN2); // Attach to stable pins
  knob.setFilter(250);
  knob.setCount(0);            // Initialize at 0

  drawSplash();
  int loopCounter = 0;
  while (loopCounter < 100){
    button.loop();
    loopCounter++;

    if (button.isPressed()){
      button.loop();
      override++;
      Serial.print("Override Level: ");
      Serial.println(override);
    }
    
    delay(50);
  }
  

  // 1. Connect to WiFi
  setupNetwork();
  
  
  // 2. Fetch Data (One-time poll)
  printDisplay("Downloading", "Fetching USCCB Readings...", formattedTime);
  fetchAllReadings();

  // 3. Disconnect WiFi to save power
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi turned off.");
  
  // 4. Show the first reading found
  updateScreen();
  knob.setCount(0);
}

// ------------------------------------------------------------------
// MAIN LOOP
// ------------------------------------------------------------------
void loop() {
  button.loop(); // MUST be called constantly
  long currKnobPosition = knob.getCount() /2;

  if (currKnobPosition > lastKnobPosition){
    Serial.println("Knob turned right");
    lastKnobPosition = currKnobPosition;

        currentIndex++;
    
    // Wrap around if we hit the end
    if (currentIndex >= readingCount) {
      currentIndex = 0; 
    }
  } else if (currKnobPosition < lastKnobPosition){
    Serial.println("Knob turned right");
    lastKnobPosition = currKnobPosition;

        currentIndex--;
    
    // Wrap around if we hit the end
    if (currentIndex < 0) {
      currentIndex = readingCount - 1; 
    }
  }
  

  if(button.isPressed() && showMenu == false){
    button.loop();
    // Implement menu to select date, reset wifi, reload readings, etc.
    showMenu = true;
    menuIndex = 0;
    Serial.println("Menu button pressed");
  }

  // Always update screen at end of loop
  updateScreen();
}


void setupNetwork() {
  // 1. Set WiFi to Station mode and Enable Flash Saving
  WiFi.mode(WIFI_STA);
  WiFi.persistent(true);

  // 2. Check if we have saved credentials
  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_STA, &conf);
  
  String savedSSID = String(reinterpret_cast<const char*>(conf.sta.ssid));
  String savedPass = String(reinterpret_cast<const char*>(conf.sta.password));

  bool connected = false;

  // 3. Try connecting with saved credentials first
  if (savedSSID.length() > 0 && override != 3) {
    Serial.print("Found saved credentials for: ");
    Serial.println(savedSSID);
    printDisplay_Connecting("Connecting...", "Using previous network", savedSSID, true);
    delay(1000);
    
    WiFi.begin(); // Uses saved credentials
    
    // Wait up to 10 seconds for connection
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
      delay(500);
      Serial.print(".");
      retry++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
    } else {
      Serial.println("\nFailed to connect with saved details.");
    }
  }

  // 4. If not connected, ask user for input using the Encoder
  if (!connected) {
    Serial.println("Starting Manual Input Mode...");
    
    String ssid = scanAndSelectWiFi();
    
    // OR: Ask user to select SSID (Simplified for now to just password)
    // If you need to scan networks, let me know!

    printDisplay("WiFi Setup", "Click to start", "Hold to submit");
    while(!button.isPressed()) button.loop(); // Wait for click
    
    // CALL YOUR NEW FUNCTION HERE
    String password = getUserHardwareInput("Password: "); 
    
    printDisplay_Connecting("WiFi Setup", "Connecting...", ssid, true);
    
    // 5. Connect AND Save to Flash
    WiFi.begin(ssid.c_str(), password.c_str()); 
    
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
  }

  // 5. Success
  Serial.println("\nWiFi Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  printDisplay_Connecting("WiFi Connected!", "Syncing Time...", "", true);
  
  // Time Sync Logic
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    formattedTime = "Date Error";
  } else {
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%A, %b %d %Y", &timeinfo);
    formattedTime = String(buffer);
    strftime(buffer, sizeof(buffer), "%m%d%y", &timeinfo);
    formattedTimeNum = String(buffer);
  }
}

// ------------------------------------------------------------------
// WIFI SELECTION MENU
// ------------------------------------------------------------------
String scanAndSelectWiFi() {
  // 1. Draw "Scanning..." Screen
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_haxrcorp4089_tr);
  u8g2.drawStr(30, 13, "Scanning...");
  u8g2.drawXBM(0, 0, 19, 16, image_wifi_bits); // Your WiFi Icon
  u8g2.sendBuffer();

  // 2. Perform Scan
  Serial.println("Scanning for networks...");
  int n = WiFi.scanNetworks();
  Serial.printf("Found %d networks\n", n);

  if (n == 0) {
    printDisplay("Error", "No Networks Found", "Reset to retry");
    while(true); // Halt
  }

  // 3. Selection Loop
  knob.setCount(0);
  int selectedIndex = 0;
  int topVisibleIndex = 0; // For scrolling viewport
  long lastKnobCount = -999;
  const int linesPerScreen = 3; // How many networks fit on screen

  while (true) {
    button.loop();

    // --- ENCODER LOGIC ---
    long rawCount = knob.getCount();
    long dividedCount = rawCount / 2; // /2 for standard detents
    
    // Calculate new index
    if (dividedCount != lastKnobCount) {
        // Handle wrap-around manually for the list
        if (dividedCount > lastKnobCount) selectedIndex++;
        else selectedIndex--;

        // Clamp selection to list bounds
        if (selectedIndex >= n) selectedIndex = 0;
        if (selectedIndex < 0) selectedIndex = n - 1;

        // --- VIEWPORT LOGIC (Scrolling) ---
        // If we scrolled off the bottom, move the view down
        if (selectedIndex >= topVisibleIndex + linesPerScreen) {
          topVisibleIndex = selectedIndex - linesPerScreen + 1;
        }
        // If we scrolled off the top, move the view up
        if (selectedIndex < topVisibleIndex) {
          topVisibleIndex = selectedIndex;
        }

        lastKnobCount = dividedCount;
    }

    // --- DRAW UI ---
    u8g2.clearBuffer();
    
    // Header
    u8g2.setFont(u8g2_font_haxrcorp4089_tr);
    u8g2.drawStr(24, 10, "Select WiFi");
    u8g2.drawXBM(0, 0, 19, 16, image_wifi_bits); // Icon Top-Left
    u8g2.drawLine(0, 16, 128, 16); // Divider Line

    // List
    u8g2.setFont(u8g2_font_6x10_tr);
    int yPos = 28; // Start Y position for first item

    for (int i = topVisibleIndex; i < topVisibleIndex + linesPerScreen; i++) {
      if (i >= n) break; // Safety break

      // Format: "Name (RSSI)"
      String ssid = WiFi.SSID(i);
      int32_t rssi = WiFi.RSSI(i);
      
      // Truncate long names to fit
      if (ssid.length() > 14) ssid = ssid.substring(0, 14) + "..";

      // Draw Selection Box (Inverted colors for selected item)
      if (i == selectedIndex) {
        u8g2.setDrawColor(1);
        u8g2.drawBox(0, yPos - 9, 128, 12); // Highlight Box
        u8g2.setDrawColor(0); // Text becomes black
      } else {
        u8g2.setDrawColor(1); // Normal white text
      }

      // Draw Text
      u8g2.drawStr(4, yPos, ssid.c_str());
      
      // Draw Signal Strength (Small number on right)
      u8g2.setCursor(100, yPos);
      u8g2.print(rssi);

      u8g2.setDrawColor(1); // Reset color for next loop
      yPos += 14; // Move down
    }

    // Draw Scrollbar Indicator (Optional visuals)
    // Just a simple dot to show where we are relative to total
    int scrollHeight = 64 - 18;
    int scrollY = 18 + (selectedIndex * scrollHeight / n);
    u8g2.drawBox(126, scrollY, 2, 4);

    u8g2.sendBuffer();


    // --- SELECTION LOGIC ---
    if (button.isPressed()) {
      button.loop();
      String chosenSSID = WiFi.SSID(selectedIndex);
      
      // Visual Feedback
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_haxrcorp4089_tr);
      u8g2.drawStr(10, 30, "Selected:");
      u8g2.drawStr(10, 50, chosenSSID.c_str());
      u8g2.sendBuffer();
      delay(1000);
      
      return chosenSSID;
    }
  }
}

// ------------------------------------------------------------------
// DISPLAY FUNCTIONS
// ------------------------------------------------------------------
void updateScreen() {

  // First check to display Menu
  if (showMenu = true){
    

  } else{
    // Update display with reading
    if (readingCount == 0) {
    printDisplay_Connecting("ERROR", "No Readings found","St. Anthony, Pray for Us", false);
    return;
  }
  
  // Print to Serial for debugging
  Serial.print("Displaying [");
  Serial.print(currentIndex);
  Serial.print("]: ");
  Serial.println(dailyReadings[currentIndex].name);

  // Draw on OLED
  printDisplay(dailyReadings[currentIndex].name, dailyReadings[currentIndex].address, formattedTime);
  }
  
}

// ------------------------------------------------------------------
// DISPLAY FUNCTIONS
// ------------------------------------------------------------------

void printDisplay(String line1, String line2, String date) {
  u8g2.clearBuffer();
  
  // Line 1: Bold Header
  u8g2.setFont(u8g2_font_haxrcorp4089_tr);
  u8g2.drawStr(30, 13, line1.c_str());
  
  // Check if reading address would overflow screen, split if needed
  if(u8g2.getStrWidth(line2.c_str()) > 126) {
    u8g2.drawStr(1, 32, line2.substring(0, line2.substring(0, 20).lastIndexOf(",") + 1).c_str());
    u8g2.drawStr(1, 44, line2.substring(line2.substring(0, 20).lastIndexOf(",")+1, line2.length()).c_str());
  } else {
    u8g2.drawStr(1, 38, line2.c_str());
  }

  u8g2.drawLine(0, 20, 127, 20);
  u8g2.drawXBM(0, 0, 17, 16, image_phone_book_open_bits);

  u8g2.drawLine(0, 50, 127, 50);
  u8g2.drawStr(20, 61, date.c_str());
  u8g2.sendBuffer();

}

void printDisplay_Connecting(String status, String step, String wifiName, bool showWifiIcon) {
u8g2.clearBuffer();
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setFont(u8g2_font_haxrcorp4089_tr);
    u8g2.drawStr(30, 13, status.c_str());

    u8g2.drawStr(1, 31, step.c_str());

    u8g2.drawLine(0, 20, 127, 20);

    u8g2.drawLine(0, 50, 127, 50);

    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(1, 42, wifiName.c_str());

    if (showWifiIcon)
      u8g2.drawXBM(0, 0, 19, 16, image_wifi_bits);

    u8g2.sendBuffer();

}

void drawSplash(void) {
    u8g2.clearBuffer();
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.drawXBM(64, -3, 64, 64, image_bible_icon_svg_bits);

    u8g2.setFont(u8g2_font_t0_18b_tr);
    u8g2.drawStr(0, 34, "Daily");

    u8g2.drawStr(0, 55, "Readings");

    u8g2.drawStr(0, 14, "The");

    u8g2.sendBuffer();
}

// ------------------------------------------------------------------
// SCRAPER ENGINE
// ------------------------------------------------------------------
void fetchAllReadings() {
  
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure(); // Ignore SSL certificate errors
    
    HTTPClient http;
    // Set a timeout for the connection
    http.setTimeout(10000); 
    http.setUserAgent("Mozilla/5.0 (ESP32)");
    
    Serial.println("Starting HTTP GET...");
    // formattedTimeNum = "012626"; // e.g., "012526" - Test Date
    if (http.begin(client, serverUrl+formattedTimeNum+".cfm")) { // Full URL with date
      int httpCode = http.GET();
      Serial.print("HTTP Code: ");
      Serial.println(httpCode);

      if (httpCode == HTTP_CODE_OK) {
        WiFiClient *stream = http.getStreamPtr();
        
        String currentTagName = "";
        bool lookingForAddress = false;
        
        // Timeout variables for the stream loop
        unsigned long lastByteTime = millis();
        const unsigned long TIMEOUT_MS = 5000; 

        Serial.println("Parsing Stream...");

        while (http.connected() && (stream->available() > 0 || stream->connected())) {
          
          // SAFETY: Break if stream hangs for 5 seconds
          if (millis() - lastByteTime > TIMEOUT_MS) {
            Serial.println("Error: Stream timed out.");
            break;
          }

          if (stream->available()) {
            lastByteTime = millis(); // Reset timeout timer
            
            String line = stream->readStringUntil('\n');
            line.trim();
            
            // --- STATE 1: FIND NAME (e.g., "Gospel") ---
            if (!lookingForAddress) {
              if (line.indexOf("<h3 class=\"name\">") >= 0) {
                // Extract name between tags
                int start = line.indexOf(">") + 1;
                int end = line.indexOf("</h3>");
                currentTagName = line.substring(start, end);
                
                Serial.println("--------------------------------");
                Serial.print("FOUND SECTION: ");
                Serial.println(currentTagName);
                lookingForAddress = true;
              }
            }
            
            // --- STATE 2: FIND ADDRESS (e.g., "John 3:16") ---
            else {
              if (line.indexOf("div class=\"address\"") >= 0) {
                 Serial.println("  -> Found Address Div! Scanning multi-line...");
                 
                 // 1. Capture FULL HTML BLOCK (Fixes blank string issue)
                 String rawHtml = line;
                 int safetyCount = 0;
                 
                 // Keep reading lines until we find the closing div
                 while (rawHtml.indexOf("</div>") == -1 && safetyCount < 20) {
                   if (stream->available()) {
                     String nextLine = stream->readStringUntil('\n');
                     rawHtml += " " + nextLine; 
                     safetyCount++;
                   } else {
                     delay(10);
                   }
                 }

                 // 2. Strip HTML Tags manually
                 String cleanAddress = "";
                 bool insideTag = false;
                 
                 for (unsigned int i=0; i < rawHtml.length(); i++) {
                   char c = rawHtml[i];
                   if (c == '<') {
                     insideTag = true;
                   } else if (c == '>') {
                     insideTag = false;
                   } else if (!insideTag) {
                     // Filter: Only allow visible characters (ASCII 32-126)
                     if (c >= 32 && c <= 126) { 
                       cleanAddress += c;
                     }
                   }
                 }
                 cleanAddress.trim(); // Remove leading/trailing spaces
                 
                 Serial.print("  -> Cleaned Text: '");
                 Serial.print(cleanAddress);
                 Serial.println("'");
                 
                 // 3. Save to memory
                 if (cleanAddress.length() > 0 && readingCount < 5 && currentTagName != "Alleluia") {
                   dailyReadings[readingCount].name = currentTagName;
                   dailyReadings[readingCount].address = cleanAddress;
                   readingCount++;
                 }

                 lookingForAddress = false; // Reset to look for next section
              }
            }
          } else {
            delay(10); // Wait for buffer to fill
          }
        }
        Serial.println("Stream Finished.");
        
      } else {
        Serial.print("HTTP Error: ");
        Serial.println(httpCode);
      }
      http.end();
    } else {
      Serial.println("Connection Failed.");
    }
  }
}

// String getUserHardwareInput(){
//   bool pressedSubmit = false;
//   char userInput[32] = "";
//   long knobPosition = 0;

//   Serial.println("\nPlease enter input using the knob and press button to submit:");

//   long refTime = millis();
//   while(pressedSubmit == false){
//     // All below is in loop
//     button.loop();
//     long newKnobposition = knob.getCount();
//     if (newKnobposition != knobPosition && (millis() - refTime) > 100) { // Debounce 100ms
//       refTime = millis();
//       Serial.print("Knob Position: ");
//       Serial.println(newKnobposition);
//       // Append character to input string
//       char inputChar = map(newKnobposition, 0, 100, 65, 90); // Map to ASCII A-Z
//       strncat(userInput, &inputChar, 1);
//       Serial.print("Current Input: ");
//       Serial.print(userInput);
//       Serial.println();
//       knobPosition = newKnobposition;
//     }
//     if (button.isPressed()) {
//       button.loop();
//       Serial.println("Submit Button Pressed");
//       pressedSubmit = true;
//     }
//   }
//   return String(userInput);
// }

String getUserHardwareInput(String prompt) {
  String userInput = "";
  
  // Encoder Init
  knob.setCount(0); 
  long lastIndex = -999; 
  
  // Button Timing Variables
  unsigned long pressStartTime = 0;
  bool isHolding = false;

  const char* charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*";
  int charsetLen = strlen(charset);

  Serial.println("--- Input Mode ---");
  printDisplay("Enter " + prompt + ":", "Rotate to Select", "Hold to Save");

  bool entering = true;
  while (entering) {
    button.loop();

    // ------------------------------------------
    // 1. ENCODER LOGIC (Standard Accumulator)
    // ------------------------------------------
    long rawCount = knob.getCount();
    long dividedCount = rawCount / 2; // Adjust to / 1 if too slow
    int newIndex = dividedCount % charsetLen;
    if (newIndex < 0) newIndex += charsetLen;

    if (newIndex != lastIndex) {
      char hoveringChar = charset[newIndex];
      printDisplay("Enter " + prompt + ":", String("Char: ") + hoveringChar, userInput + "_");
      lastIndex = newIndex;
    }

    // ------------------------------------------
    // 2. BUTTON LOGIC (Short vs Long Press)
    // ------------------------------------------
    
    // A. Button Start Press
    if (button.isPressed()) {
      pressStartTime = millis();
      isHolding = true;
    }

    // B. Check while Button is held down
    if (isHolding && button.getState() == LOW) { // LOW usually means pressed for ezButton
      unsigned long holdTime = millis() - pressStartTime;
      
      // If held for more than 2 seconds (2000ms)
      if (holdTime > 2000) {
        Serial.println("Long Press Detected - Saving!");
        printDisplay("SAVED!", "Password set.", "");
        delay(1000); 
        entering = false; // BREAK THE LOOP
        isHolding = false; 
      }
    }

    // C. Button Release (Short Click)
    if (button.isReleased()) {
      // Only add char if we didn't just trigger the long press save
      if (isHolding) {
        unsigned long duration = millis() - pressStartTime;
        
        if (duration < 2000) {
          // It was a short click -> Add Character
          char selectedChar = charset[newIndex];
          userInput += selectedChar;
          
          printDisplay("ADDED", String(selectedChar), userInput);
          delay(750); 
          lastIndex = -999; // Force screen refresh
        }
        isHolding = false; // Reset flag
      }
    }
    
    delay(10); 
  }
  
  return userInput;
}