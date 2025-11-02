#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "nfl_logos.h"  // Include the logo header file
#include <Fonts/TomThumb.h>

// ===== WiFi Configuration =====
const char* ssid = "a wifi has no name (deco)";
const char* password = "5736946039";

// ===== Raspberry Pi/Mac Server =====
const char* piServerUrl = "http://192.168.68.50:5001/nfl/scores";

// ===== LED Matrix Configuration =====
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 2

// Pin configuration
#define R1_PIN 25
#define G1_PIN 26
#define B1_PIN 27
#define R2_PIN 14
#define G2_PIN 12
#define B2_PIN 13
#define A_PIN 23
#define B_PIN 19
#define C_PIN 5
#define D_PIN 17
#define E_PIN 18
#define LAT_PIN 4
#define OE_PIN 15
#define CLK_PIN 16

// ===== Matrix Display Setup =====
MatrixPanel_I2S_DMA *dma_display = nullptr;

// ===== Color Definitions =====
uint16_t COLOR_WHITE;
uint16_t COLOR_RED;
uint16_t COLOR_GREEN;
uint16_t COLOR_BLUE;
uint16_t COLOR_YELLOW;
uint16_t COLOR_ORANGE;

// ===== Game Data Structure =====
struct Game {
  String homeTeam;
  String awayTeam;
  int homeScore;
  int awayScore;
  String detail;
  bool isLive;
  bool isFinal;
  bool isUpcoming;
  String possession;  // Add this - will be "home", "away", or ""
};

Game games[16];
int gameCount = 0;
int currentGameIndex = 0;
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 30000; // 30 seconds

// ===== Function Forward Declarations =====
void connectToWiFi();
void fetchScoresFromServer();
void parseSimpleJSON(String jsonData);
void displayGame(Game &game);
void displayMessage(String message, uint16_t color);
void drawBitmap(int x, int y, const uint8_t *bitmap, int w, int h, uint16_t color);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println("NFL Score Ticker - Server Mode");
  Serial.println("========================================");

  // Initialize LED Matrix
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN);
  
  mxconfig.gpio.r1 = R1_PIN;
  mxconfig.gpio.g1 = G1_PIN;
  mxconfig.gpio.b1 = B1_PIN;
  mxconfig.gpio.r2 = R2_PIN;
  mxconfig.gpio.g2 = G2_PIN;
  mxconfig.gpio.b2 = B2_PIN;
  mxconfig.gpio.a = A_PIN;
  mxconfig.gpio.b = B_PIN;
  mxconfig.gpio.c = C_PIN;
  mxconfig.gpio.d = D_PIN;
  mxconfig.gpio.e = E_PIN;
  mxconfig.gpio.lat = LAT_PIN;
  mxconfig.gpio.oe = OE_PIN;
  mxconfig.gpio.clk = CLK_PIN;

  // Anti-flicker settings (optional, uncomment if you have flickering)
  // mxconfig.clkphase = false;
  // mxconfig.driver = HUB75_I2S_CFG::FM6126A;
  // mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  // mxconfig.latch_blanking = 4;
  // mxconfig.double_buff = true;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(90);
  dma_display->clearScreen();

  // Initialize colors
  COLOR_WHITE = dma_display->color565(255, 255, 255);
  COLOR_RED = dma_display->color565(255, 0, 0);
  COLOR_GREEN = dma_display->color565(0, 0, 0);
  COLOR_BLUE = dma_display->color565(0, 0, 255);
  COLOR_YELLOW = dma_display->color565(255, 255, 0);
  COLOR_ORANGE = dma_display->color565(255, 165, 0);

  Serial.println("Display initialized");

  // Connect to WiFi
  displayMessage("Connecting...", COLOR_YELLOW);
  connectToWiFi();
  
  if (WiFi.status() == WL_CONNECTED) {
    displayMessage("Connected!", COLOR_GREEN);
    delay(1000);
    fetchScoresFromServer();
  }
}

void loop() {
  // Update scores periodically
  if (millis() - lastUpdate > updateInterval) {
    fetchScoresFromServer();
    lastUpdate = millis();
  }

  // Cycle through games
  if (gameCount > 0) {
    displayGame(games[currentGameIndex]);
    delay(5000); // Show each game for 5 seconds
    
    currentGameIndex++;
    if (currentGameIndex >= gameCount) {
      currentGameIndex = 0;
    }
  } else {
    displayMessage("No games", COLOR_YELLOW);
    delay(5000);
  }
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Server URL: ");
    Serial.println(piServerUrl);
  } else {
    Serial.println("\n✗ WiFi Failed!");
  }
}

void fetchScoresFromServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    return;
  }

  Serial.println("\n--- Fetching from Server ---");

  HTTPClient http;
  http.begin(piServerUrl);
  http.setTimeout(10000);
  
  int httpCode = http.GET();
  
  Serial.print("HTTP Code: ");
  Serial.println(httpCode);

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.print("Response size: ");
    Serial.print(payload.length());
    Serial.println(" bytes");
    
    parseSimpleJSON(payload);
    
    Serial.print("Successfully parsed ");
    Serial.print(gameCount);
    Serial.println(" games");
    
  } else {
    Serial.print("✗ Error: ");
    Serial.println(httpCode);
    displayMessage("Server Error", COLOR_RED);
    delay(2000);
  }

  http.end();
}

void parseSimpleJSON(String jsonData) {
  Serial.println("Parsing simplified JSON...");
  
  DynamicJsonDocument doc(8192);
  
  DeserializationError error = deserializeJson(doc, jsonData);

  if (error) {
    Serial.print("✗ JSON error: ");
    Serial.println(error.c_str());
    return;
  }

  Serial.println("✓ JSON parsed successfully");

  gameCount = 0;
  JsonArray gamesArray = doc["games"];
  
  Serial.print("Games in response: ");
  Serial.println(gamesArray.size());

  for (JsonObject gameObj : gamesArray) {
    if (gameCount >= 16) break;

    Game &game = games[gameCount];
    
    game.awayTeam = gameObj["away"]["abbr"] | "???";
    game.homeTeam = gameObj["home"]["abbr"] | "???";
    
    // Handle scores as strings and convert to int
    String awayScoreStr = gameObj["away"]["score"] | "0";
    String homeScoreStr = gameObj["home"]["score"] | "0";
    game.awayScore = awayScoreStr.toInt();
    game.homeScore = homeScoreStr.toInt();
    
    game.detail = gameObj["status"]["detail"] | "";
    game.isLive = gameObj["live"] | false;
    game.isFinal = gameObj["final"] | false;
    game.isUpcoming = gameObj["upcoming"] | false;
    game.possession = gameObj["possession"] | "";

    Serial.print("Game ");
    Serial.print(gameCount + 1);
    Serial.print(": ");
    Serial.print(game.awayTeam);
    Serial.print(" ");
    Serial.print(game.awayScore);
    Serial.print(" @ ");
    Serial.print(game.homeTeam);
    Serial.print(" ");
    Serial.print(game.homeScore);
    
    if (game.isLive) {
      Serial.print(" [LIVE]");
    } else if (game.isFinal) {
      Serial.print(" [FINAL]");
    } else if (game.isUpcoming) {
      Serial.print(" [UPCOMING]");
    }
    Serial.println();

    gameCount++;
  }
}

void drawBitmap(int x, int y, const uint8_t *bitmap, int w, int h, uint16_t color) {
  for (int j = 0; j < h; j++) {
    uint8_t line = pgm_read_byte(&bitmap[j]);
    for (int i = 0; i < w; i++) {
      if (line & (0x80 >> i)) {
        dma_display->drawPixel(x + i, y + j, color);
      }
    }
  }
}

void drawColorBitmap(int x, int y, const uint16_t *bitmap, int w, int h) {
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      uint16_t color = pgm_read_word(&bitmap[j * w + i]);
      dma_display->drawPixel(x + i, y + j, color);
    }
  }
}

void displayGame(Game &game) {
  dma_display->clearScreen();
  
  // Draw away team logo (left side, 32x32)
  const uint16_t* awayLogo = getTeamLogo(game.awayTeam);
  drawColorBitmap(2, 0, awayLogo, 32, 32);
  
  // Draw football next to away team if they have possession
  if (game.possession == "away" || game.possession == game.awayTeam) {
    drawBitmap(35, 18, FOOTBALL_ICON, 8, 5, COLOR_YELLOW);
  }
  
  // Draw home team logo (right side, 32x32)
  const uint16_t* homeLogo = getTeamLogo(game.homeTeam);
  drawColorBitmap(96, 0, homeLogo, 32, 32);
  
  // Draw football next to home team if they have possession
  if (game.possession == "home" || game.possession == game.homeTeam) {
    drawBitmap(85, 18, FOOTBALL_ICON, 8, 5, COLOR_YELLOW);
  }
  
  // MIDDLE: Score (centered)
  dma_display->setFont();
  dma_display->setTextSize(2);
  String scoreText = String(game.awayScore) + "-" + String(game.homeScore);
  int textWidth = scoreText.length() * 10;
  int xPos = (128 - textWidth - 4) / 2;
  dma_display->setCursor(xPos, 2);
  dma_display->setTextColor(COLOR_WHITE);
  dma_display->print(scoreText);
  
  // Separator
  // for (int x = 0; x < 128; x++) {
  //   dma_display->drawPixel(x, 14, COLOR_YELLOW);
  // }
  
  // BOTTOM: Status (centered)
  String statusText = "";
  uint16_t statusColor;
  
  if (game.isLive) {
    statusText = "LIVE";
    statusColor = COLOR_GREEN;
  } else if (game.isFinal) {
    statusText = "FINAL";
    statusColor = COLOR_RED;
  } else if (game.isUpcoming) {
    statusText = "UPCOMING";
    statusColor = COLOR_BLUE;
  } else {
    statusText = "SCHEDULED";
    statusColor = COLOR_ORANGE;
  }
  
  // int statusWidth = statusText.length() * 3;
  // Serial.println(statusText.length());
  // int statusXPos = (128 - statusWidth) / 2;
  // dma_display->setFont(&TomThumb);
  // dma_display->setCursor(statusXPos, 22);
  // dma_display->print(statusText);
  
  // Detail
  
  String detailShort = game.detail;
  if (detailShort.length() > 8) {
    dma_display->setTextSize(1);
    dma_display->setFont(&TomThumb);
    detailShort = detailShort.substring(0, 30);
    int detailWidth = detailShort.length() * 3;
    int detailXPos = (128 - detailWidth - 4) / 2;
    dma_display->setCursor(detailXPos, 30);
    dma_display->setTextColor(COLOR_YELLOW);
    dma_display->print(detailShort);
  }
    else {
      statusText = "SCHEDULED";
      statusColor = COLOR_ORANGE;
      dma_display->setTextSize(2);
      dma_display->setFont(&TomThumb);
      detailShort = detailShort.substring(0, 30);
      int detailWidth = detailShort.length() * 6;
      int detailXPos = (128 - detailWidth) / 2;
      dma_display->setCursor(detailXPos, 30);
      dma_display->setTextColor(COLOR_RED);
      dma_display->print(detailShort);
    }

}

void displayMessage(String message, uint16_t color) {
  dma_display->clearScreen();
  dma_display->setCursor(2, 12);
  dma_display->setTextColor(color);
  dma_display->print(message);
}