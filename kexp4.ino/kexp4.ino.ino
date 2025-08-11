// KEXP Now Playing E-ink Display with Night Mode
// Based on GxEPD2_HelloWorld.ino by Jean-Marc Zingg

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_4C.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// WiFi credentials
const char* ssid = "Kingman Hardware";
const char* password = "keepmovingforward";

// Display setup
#include "GxEPD2_display_selection_new_style.h"

// NTP setup for time checking
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -8 * 3600, 60000); // PST timezone (-8 hours)

// Global variables for track info
String currentArtist = "Loading...";
String currentSong = "Loading...";
String currentAlbum = "Loading...";
String releaseYear = "";
String lastUpdate = "";
bool isLocal = false;
bool isAirbreak = false;
int updateCounter = 0; // Track number of updates for periodic full refresh

// Previous values for comparison (to detect changes)
String prevArtist = "";
String prevSong = "";
String prevAlbum = "";
String prevYear = "";
bool prevIsLocal = false;
bool prevIsAirbreak = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== KEXP Display Starting ===");
  
  // Initialize display
  display.init(115200, true, 2, false);
  
  // Connect to WiFi
  connectToWiFi();
  
  // Initialize time client
  timeClient.begin();
  timeClient.update();
  Serial.println("Current time: " + timeClient.getFormattedTime());
  
  // Try to get track info
  if (updateKEXPInfo()) {
    Serial.println("Successfully got track info!");
  } else {
    Serial.println("Failed to get track info, using demo data");
    currentArtist = "Demo Artist";
    currentSong = "Demo Song";
    currentAlbum = "Demo Album";
    releaseYear = "2024";
    lastUpdate = "Demo";
  }
  
  // Display the info
  displayNowPlaying();
  display.hibernate();
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  
  // Set WiFi to station mode and enable power saving
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Enable WiFi power saving mode
    WiFi.setSleepMode(WIFI_MODEM_SLEEP);
    Serial.println("WiFi power saving mode enabled");
  } else {
    Serial.println("\nWiFi connection failed!");
  }
}

bool hasContentChanged() {
  // Check if any of the display content has changed
  return (currentArtist != prevArtist || 
          currentSong != prevSong || 
          currentAlbum != prevAlbum || 
          releaseYear != prevYear || 
          isLocal != prevIsLocal);
}

void updatePreviousValues() {
  // Store current values as previous for next comparison
  prevArtist = currentArtist;
  prevSong = currentSong;
  prevAlbum = currentAlbum;
  prevYear = releaseYear;
  prevIsLocal = isLocal;
  prevIsAirbreak = isAirbreak;
}

bool isNightTime() {
  timeClient.update();
  int currentHour = timeClient.getHours();
  
  // Night mode: 11pm (23) to 6am (6)
  return (currentHour >= 23 || currentHour < 6);
}

void displayNightMode() {
  Serial.println("\n=== Night Mode: GO TO BED BOY! ===");
  
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow(); // Use full refresh for night mode message
  display.firstPage();
  
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // Center the night message
    display.setFont(&FreeSansBold12pt7b);
    int16_t nightTbx, nightTby;
    uint16_t nightTbw, nightTbh;
    display.getTextBounds("GO TO BED BOY!", 0, 0, &nightTbx, &nightTby, &nightTbw, &nightTbh);
    
    // Calculate center position
    int x = (display.width() - nightTbw) / 2 - nightTbx;
    int y = (display.height() - nightTbh) / 2 - nightTby;
    
    display.setCursor(x, y);
    display.print("GO TO BED BOY!");
    
  } while (display.nextPage());
  
  Serial.println("Night mode message displayed!");
}

bool updateKEXPInfo() {
  Serial.println("\n=== Starting KEXP API Call ===");
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ERROR: WiFi not connected");
    // Display centered error state
    displayErrorState("connecting...");
    return false;
  }
  
  // Try a simple HTTP request first (without SSL)
  WiFiClient client;
  HTTPClient http;
  
  // Use a simple test API first
  String testUrl = "http://httpbin.org/get";
  Serial.println("Testing basic HTTP with: " + testUrl);
  
  http.begin(client, testUrl);
  http.addHeader("User-Agent", "ESP8266-Test");
  
  int httpCode = http.GET();
  Serial.print("Test HTTP Response: ");
  Serial.println(httpCode);
  
  if (httpCode == 200) {
    Serial.println("Basic HTTP works! Now trying KEXP...");
    http.end();
    
    // Now try KEXP with SSL
    WiFiClientSecure secureClient;
    secureClient.setInsecure();
    
    String kexpUrl = "https://api.kexp.org/v2/plays/?limit=1";
    Serial.println("Trying KEXP API: " + kexpUrl);
    
    http.begin(secureClient, kexpUrl);
    http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    http.addHeader("Accept", "application/json");
    
    httpCode = http.GET();
    Serial.print("KEXP HTTP Response: ");
    Serial.println(httpCode);
    
    if (httpCode == 200) {
      String payload = http.getString();
      Serial.println("Got response, length: " + String(payload.length()));
      Serial.println("First 300 characters of response:");
      Serial.println(payload.substring(0, 300));
      
      // Try to parse JSON
      Serial.println("Attempting to parse JSON...");
      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error) {
        Serial.println("JSON parsed successfully!");
        JsonArray results = doc["results"];
        Serial.println("Number of results: " + String(results.size()));
        
        if (results.size() > 0) {
          Serial.println("Processing first result...");
          JsonObject track = results[0];
          
          // Check if it's an airbreak first
          String playType = track["play_type"].as<String>();
          Serial.println("Play type: " + playType);
          
          if (playType == "airbreak") {
            Serial.println("Detected airbreak");
            isAirbreak = true;
            currentArtist = "";
            currentSong = "";
            currentAlbum = "";
            releaseYear = "";
            isLocal = false;
            lastUpdate = "Live";
            http.end();
            return true;
          }
          
          Serial.println("Extracting track data...");
          
          // Not an airbreak, so clear the airbreak flag
          isAirbreak = false;
          
          // Check if artist is local
          isLocal = track["is_local"].as<bool>();
          Serial.println("Is local artist: " + String(isLocal ? "Yes" : "No"));
          
          // Extract data with null checking
          currentArtist = track["artist"].as<String>();
          if (currentArtist == "null" || currentArtist.length() == 0) {
            currentArtist = "nada here";
          }
          
          currentSong = track["song"].as<String>();
          if (currentSong == "null" || currentSong.length() == 0) {
            currentSong = "nada here";
          }
          
          currentAlbum = track["album"].as<String>();
          if (currentAlbum == "null" || currentAlbum.length() == 0) {
            currentAlbum = "";
          }
          
          releaseYear = track["release_date"].as<String>();
          if (releaseYear == "null" || releaseYear.length() == 0) {
            releaseYear = "";
          } else if (releaseYear.length() >= 4) {
            // Extract just the year from release_date (format: "YYYY-MM-DD")
            releaseYear = releaseYear.substring(0, 4);
          }
          
          lastUpdate = "Live";
          
          Serial.println("SUCCESS! Got track data:");
          Serial.println("Artist: " + currentArtist);
          Serial.println("Song: " + currentSong);
          Serial.println("Album: " + currentAlbum);
          Serial.println("Year: " + releaseYear);
          
          http.end();
          return true;
        } else {
          Serial.println("No results found in response");
          // Display centered error state
          displayErrorState("dad not found");
        }
      } else {
        Serial.print("JSON Error: ");
        Serial.println(error.c_str());
        Serial.println("Raw response that failed to parse:");
        Serial.println(payload);
        // Display centered error state
        displayErrorState("connecting...");
      }
    } else {
      String errorResponse = http.getString();
      Serial.println("KEXP Error Response: " + errorResponse);
      // Display centered error state
      displayErrorState("connecting...");
    }
  } else {
    Serial.println("Basic HTTP test failed!");
    // Display centered error state
    displayErrorState("connecting...");
  }
  
  http.end();
  return false;
}

void displayNowPlaying() {
  Serial.println("\n=== Updating Display ===");
  
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);
  
  // Increment update counter
  updateCounter++;
  
  // Do a full refresh every 10 updates to clear any ghosting
  if (updateCounter >= 10) {
    Serial.println("Performing full refresh to clear ghosting");
    display.setFullWindow();
    updateCounter = 0; // Reset counter
  } else {
    Serial.println("Using partial refresh");
    display.setPartialWindow(0, 0, display.width(), display.height());
  }
  
  display.firstPage();
  
  do {
    display.fillScreen(GxEPD_WHITE);
    
    int y = 15;
    int x = 6;
    
    // Declare text bounds variables once for the whole function
    int16_t tbx, tby;
    uint16_t tbw, tbh;
    
    // KEXP in 9pt sans - top right (with tilde if local)
    display.setFont(&FreeSans9pt7b);
    String kexpText = isLocal ? "~ KEXP ~" : "KEXP";
    display.getTextBounds(kexpText, 0, 0, &tbx, &tby, &tbw, &tbh);
    display.setCursor(display.width() - tbw - 6, y);
    display.print(kexpText);
    
    // Song in 12pt bold sans - left side (with auto-sizing)
    y += 40;
    display.setFont(&FreeSansBold12pt7b);
    String songText = truncateString(currentSong, 18);
    
    // Check if song fits, if not use smaller font
    display.getTextBounds(songText, 0, 0, &tbx, &tby, &tbw, &tbh);
    int availableWidth = display.width() - (x * 2);
    
    if (tbw > availableWidth) {
      // Switch to smaller font for long songs
      display.setFont(&FreeSansBold9pt7b);
      songText = truncateString(currentSong, 24); // More characters with smaller font
    }
    
    display.setCursor(x, y);
    display.print(songText);
    y += 25;
    
    // Artist in 12pt bold sans - left side (with auto-sizing)
    display.setFont(&FreeSansBold12pt7b);
    String artistText = truncateString(currentArtist, 18);
    
    // Check if artist fits, if not use smaller font
    display.getTextBounds(artistText, 0, 0, &tbx, &tby, &tbw, &tbh);
    
    if (tbw > availableWidth) {
      // Switch to smaller font for long artist names
      display.setFont(&FreeSansBold9pt7b);
      artistText = truncateString(currentArtist, 24); // More characters with smaller font
    }
    
    display.setCursor(x, y);
    display.print(artistText);
    y += 25;
    
    // Album and year in 9pt sans - left side
    display.setFont(&FreeSans9pt7b);
    String albumInfo = "";
    
    // Calculate available width (display width minus margins)
    availableWidth = display.width() - (x * 2); // subtract left margin and matching right margin
    
    if (releaseYear.length() > 0) {
      // First, measure the year part (including space)
      String yearPart = " " + releaseYear;
      int16_t yearX, yearY;
      uint16_t yearW, yearH;
      display.getTextBounds(yearPart, 0, 0, &yearX, &yearY, &yearW, &yearH);
      
      // Calculate how much space is left for the album name
      int albumWidth = availableWidth - yearW;
      
      // Truncate album name to fit in remaining space
      String truncatedAlbum = currentAlbum;
      int16_t testX, testY;
      uint16_t testW, testH;
      
      while (truncatedAlbum.length() > 0) {
        display.getTextBounds(truncatedAlbum, 0, 0, &testX, &testY, &testW, &testH);
        if (testW <= albumWidth) {
          break; // It fits!
        }
        // Remove last few characters and add "..."
        if (truncatedAlbum.length() > 3) {
          truncatedAlbum = truncatedAlbum.substring(0, truncatedAlbum.length() - 4) + "...";
        } else {
          truncatedAlbum = "...";
          break;
        }
      }
      
      albumInfo = truncatedAlbum + yearPart;
    } else {
      // No year, just truncate album to fit available width
      albumInfo = truncateString(currentAlbum, 30);
    }
    
    display.setCursor(x, y);
    display.print(albumInfo);
    
  } while (display.nextPage());
  
  Serial.println("Display updated!");
}

void displayErrorState(String message) {
  Serial.println("\n=== Displaying Error State: " + message + " ===");
  
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);
  display.setFullWindow(); // Always use full refresh for error states
  display.firstPage();
  
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // Center the message on screen
    display.setFont(&FreeSans9pt7b);
    int16_t tbx, tby;
    uint16_t tbw, tbh;
    display.getTextBounds(message, 0, 0, &tbx, &tby, &tbw, &tbh);
    
    // Calculate center position
    int x = (display.width() - tbw) / 2 - tbx;
    int y = (display.height() - tbh) / 2 - tby;
    
    display.setCursor(x, y);
    display.print(message);
    
  } while (display.nextPage());
  
  Serial.println("Error state displayed!");
}

String truncateString(String str, int maxLength) {
  if (str.length() <= maxLength) {
    return str;
  }
  return str.substring(0, maxLength - 3) + "...";
}

void loop() {
  delay(30000); // Check every 30 seconds
  
  Serial.println("\n=== Loop Check ===");
  display.init(115200, true, 2, false);
  
  // Check if it's night time
  if (isNightTime()) {
    Serial.println("Night time detected (11pm-6am) - displaying night message");
    displayNightMode();
    display.hibernate();
    return; // Skip API calls during night
  }
  
  Serial.println("Day time - checking API");
  
  if (updateKEXPInfo()) {
    // API call succeeded, check if content changed
    if (hasContentChanged()) {
      Serial.println("Content changed - updating display");
      
      // Check if it's an airbreak
      if (isAirbreak) {
        displayErrorState("~ airbreak ~");
      } else {
        displayNowPlaying();
      }
      
      updatePreviousValues();
    } else {
      Serial.println("No content change - skipping display update");
    }
  } else {
    // API failed - error states are handled within updateKEXPInfo()
    // Reset previous values so next successful call will trigger update
    prevArtist = "";
    prevSong = "";
    prevAlbum = "";
    prevYear = "";
    prevIsLocal = false;
    prevIsAirbreak = false;
  }
  
  display.hibernate();
}