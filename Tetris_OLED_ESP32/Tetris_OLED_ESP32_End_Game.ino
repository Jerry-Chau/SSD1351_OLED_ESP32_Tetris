/*
 * This is an example sketch that shows how to toggle the SSD1331 OLED display
 * on and off at runtime to avoid screen burn-in.
 * 
 * The sketch also demonstrates how to erase a previous value by re-drawing the 
 * older value in the screen background color prior to writing a new value in
 * the same location. This avoids the need to call fillScreen() to erase the
 * entire screen followed by a complete redraw of screen contents.
 * 
 * Written by Phill Kelley. BSD license.
 */
 
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>
#include <SPI.h>
#include <esp_now.h>
#include <WiFi.h>

#include "Jiji_Logo.h"
#include "GameOver.h"
#include "Title.h"

#define SerialDebugging true

// Screen dimensions
#define WIDTH  128
#define HEIGHT 128 // Change this to 96 for 1.27" OLED.

// for debouncing interrupt
struct Button {
    const uint8_t PIN;
    uint32_t numberKeyPresses;
    volatile bool isButtonPressed;
};

Button Button_pin = {32, 0, false};

// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
  bool a;
  bool b;
  bool c;
  bool d;
} struct_message;

// Create a struct_message for holding incoming data
struct_message InData;

esp_now_peer_info_t peerInfo;

//variables to keep track of the timing of recent interrupts
unsigned long button_time = 0;  
unsigned long last_button_time = 0; 

// The SSD1351 is connected like this (plus VCC plus GND)
const uint8_t   OLED_pin_scl_sck        = 18;
const uint8_t   OLED_pin_sda_mosi       = 23;
const uint8_t   OLED_pin_cs_ss          = 5;
const uint8_t   OLED_pin_res_rst        = 17;
const uint8_t   OLED_pin_dc_rs          = 4;

// Button Pins
const uint8_t left=25;
const uint8_t right=26;
const uint8_t change=27;
const uint8_t speed=2;

bool empty_flag = false;

// declare the display
Adafruit_SSD1351 oled =
    Adafruit_SSD1351(
        WIDTH,
        HEIGHT,
        &SPI,
        OLED_pin_cs_ss,
        OLED_pin_dc_rs,
        OLED_pin_res_rst
     );

// SSD1331 color definitions
const uint16_t  BLACK        = 0x0000;
const uint16_t  BLUE         = 0x001F;
const uint16_t  RED          = 0xF800;
const uint16_t  GREEN        = 0x07E0;
const uint16_t  CYAN         = 0x07FF;
const uint16_t  MAGENTA      = 0xF81F;
const uint16_t  YELLOW       = 0xFFE0;
const uint16_t  WHITE        = 0xFFFF;

  const char pieces_S_l[4][2][4] = {
                                      { {1,2,0,1}, {0,0,1,1} },  // rotation 0
                                      { {0,0,1,1}, {0,1,1,2} },  // rotation 1
                                      { {1,2,0,1}, {0,0,1,1} },  // rotation 2
                                      { {0,0,1,1}, {0,1,1,2} }
                                    };

  const char pieces_S_r[4][2][4] = {
                                     { {1,2,0,1}, {0,0,1,1} },  // rotation 0
                                     { {0,0,1,1}, {0,1,1,2} },  // rotation 1
                                     { {1,2,0,1}, {0,0,1,1} },  // rotation 2
                                     { {0,0,1,1}, {0,1,1,2} }
                                    };    

  const char pieces_L_l[4][2][4] = {
    { {0,0,0,1}, {0,1,2,2} },  // rotation 0
    { {0,1,2,0}, {1,1,1,0} },  // rotation 1
    { {0,1,1,1}, {0,0,1,2} },  // rotation 2
    { {0,1,1,1}, {0,0,1,2} }   // rotation 3 (fixed)
};


  const char pieces_Sq[4][2][4] = {
                                      { {0,1,0,1}, {0,0,1,1} },
                                      { {0,1,0,1}, {0,0,1,1} },
                                      { {0,1,0,1}, {0,0,1,1} },
                                      { {0,1,0,1}, {0,0,1,1} }
                                  };
  const char pieces_T[4][2][4] = {
                                      { {1,0,1,2}, {0,1,1,1} },  // rotation 0
                                      { {0,0,1,0}, {0,1,1,2} },  // rotation 1
                                      { {0,1,2,1}, {1,1,1,2} },  // rotation 2
                                      { {1,0,1,1}, {0,1,1,2} }   // rotation 3
                                  };

  const char pieces_l[4][2][4] = {
                                      { {0,1,2,3}, {0,0,0,0} },  // rotation 0
                                      { {0,0,0,0}, {0,1,2,3} },  // rotation 1
                                      { {0,1,2,3}, {0,0,0,0} },  // rotation 2
                                      { {0,0,0,0}, {0,1,2,3} }   // rotation 3
                                  };

  const char (*pieces[7])[2][4] = {
  pieces_S_l,
  pieces_S_r,
  pieces_L_l,
  pieces_Sq,
  pieces_T,
  pieces_l,
};
  const short TYPES = 6;
  int click[] = { 1047 };
  int click_duration[] = { 100 };
  int erase[] = { 2093 };
  int erase_duration[] = { 50 };
  word currentType, nextType, rotation;
  short pieceX, pieceY;
  short piece[2][4];
  int interval = 20, score;
  long timer, delayer;
  boolean grid[10][18];
  boolean b1, b2, b3;
  const int CELL = 6;
  const int GRID_X = 34;   // left edge of grid inside rectangle
  const int GRID_Y = 17;   // top edge of grid inside rectangle

  // callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&InData, incomingData, sizeof(InData));

  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Left: ");
  Serial.println(InData.a);
  Serial.print("Right: ");
  Serial.println(InData.b);
  Serial.print("Drop: ");
  Serial.println(InData.c);
  Serial.print("Rotate: ");
  Serial.println(InData.d);
  Serial.println();
}

  void checkLines(){
    boolean full;
    for(short y = 17; y >= 0; y--){
      full = true;
      for(short x = 0; x < 10; x++){
        full = full && grid[x][y];
      }
      if(full){
        breakLine(y);
        y++;
        oled.fillRect(2, 2, 50, 12, BLACK);
        text_score();
      }
    }
  }

  void breakLine(short line){
    // shift all rows above `line` down by 1
    for (short y = line; y > 0; y--) {
        for (short x = 0; x < 10; x++) {
            grid[x][y] = grid[x][y - 1];
        }
    }

    // clear the top row
    for (short x = 0; x < 10; x++) {
        grid[x][0] = 0;
    }

    oled.invertDisplay(true);
    delay(50);
    oled.invertDisplay(false);
    score += 10;
}


  void generate() {
    currentType = nextType;
    nextType = random(TYPES);

    switch (currentType) {
        case 0: // S_l
        case 1: // S_r
        case 2: // L_l
        case 4: // T
            pieceX = random(8);  // 0–7
            break;

        case 3: // Square
            pieceX = random(9);  // 0–8
            break;

        case 5: // I piece
            pieceX = random(7);  // 0–6
            break;
    }

    pieceY = -2;
    rotation = 0;
    copyPiece(piece, currentType, rotation);
}


  void drawPiece(short x, short y, uint16_t color, const short piece[2][4]){
    for (short i = 0; i < 4; i++) {

        short px = x + piece[0][i];
        short py = y + piece[1][i];

        // Skip blocks above the visible grid
        if (py < 0) continue;

        oled.fillRect(
            GRID_X + px * CELL,
            GRID_Y + py * CELL,
            CELL,
            CELL,
            color
        );
    }
  }

  void drawNextPiece() {
    short nPiece[2][4];

    short previewRot = (nextType == 5) ? 1 : 0;   // type 5 = I piece
    copyPiece(nPiece, nextType, previewRot);

    const int PREVIEW_X = 100;
    const int PREVIEW_Y = 18;
    const int PREVIEW_CELL = 4;

    for (short i = 0; i < 4; i++) {
        int px = PREVIEW_X + 8 + nPiece[0][i] * PREVIEW_CELL;
        int py = PREVIEW_Y + 10 + nPiece[1][i] * PREVIEW_CELL;

        oled.fillRect(px, py, PREVIEW_CELL, PREVIEW_CELL, YELLOW);
    }
}
  void drawBlankNextPiece(){
    oled.fillRect(105, 25, 18, 25, BLACK);
  }
  void copyPiece(short piece[2][4], short type, short rotation){
    switch(type){
    case 0: //L_l
      for(short i = 0; i < 4; i++){
        piece[0][i] = pieces_L_l[rotation][0][i];
        piece[1][i] = pieces_L_l[rotation][1][i];
      }
      break;
    case 1: //S_l
      for(short i = 0; i < 4; i++){
        piece[0][i] = pieces_S_l[rotation][0][i];
        piece[1][i] = pieces_S_l[rotation][1][i];
      }
      break;
    case 2: //S_r
      for(short i = 0; i < 4; i++){
        piece[0][i] = pieces_S_r[rotation][0][i];
        piece[1][i] = pieces_S_r[rotation][1][i];
      }
      break;
    case 3: //Sq
      for(short i = 0; i < 4; i++){
        piece[0][i] = pieces_Sq[0][0][i];
        piece[1][i] = pieces_Sq[0][1][i];
      }
      break;
      case 4: //T
      for(short i = 0; i < 4; i++){
        piece[0][i] = pieces_T[rotation][0][i];
        piece[1][i] = pieces_T[rotation][1][i];
      }
      break;
      case 5: //l
      for(short i = 0; i < 4; i++){
        piece[0][i] = pieces_l[rotation][0][i];
        piece[1][i] = pieces_l[rotation][1][i];
      }
      break;
    }
  }
  short getMaxRotation(short type){
    if (type == 3)   // Square
        return 1;
    return 4;        // All other pieces have 4 rotations
  }

  bool canRotate(short rotation) {
    short test[2][4];
    copyPiece(test, currentType, rotation);

    for (int i = 0; i < 4; i++) {
        int x = pieceX + test[0][i];
        int y = pieceY + test[1][i];

        // Ignore blocks above the screen
        if (y < 0)
            continue;

        // Out of bounds horizontally
        if (x < 0 || x > 9)
            return false;

        // Below bottom
        if (y > 17)
            return false;

        // Collision with grid
        if (grid[x][y])
            return false;
    }

    return true;
}


  void text_score(){
    char text[6];
    itoa(score, text, 10);
    drawText(text, getNumberLength(score), 7, 4);
  }

  void drawLayout(){
    oled.drawRect(33, 15, WIDTH - 65, HEIGHT - 15, CYAN);
    oled.drawLine(0, 15, WIDTH, 15, WHITE);
    oled.drawRect(0, 0, WIDTH, HEIGHT, WHITE);
  }

  short getNumberLength(int n){
    short counter = 1;
    while(n >= 10){
      n /= 10;
      counter++;
    }
    return counter;
  }

  void drawText(char text[], short length, int x, int y){
    oled.setTextSize(1);      // Normal 1:1 pixel scale
    oled.setTextColor(WHITE); // Draw white text
    oled.setCursor(x, y);     // Start at top-left corner
    oled.cp437(true);         // Use full 256 char 'Code Page 437' font
    for(short i = 0; i < length; i++)
      oled.write(text[i]);
  }

// assume the display is off until configured in setup()
bool            isDisplayVisible        = false;

void IRAM_ATTR isr() {
    button_time = millis();
    if (button_time - last_button_time > 250)
    {
        Button_pin.numberKeyPresses++;
        Button_pin.isButtonPressed = true;
        last_button_time = button_time;
    }
}

void displayJiJi() {
    oled.drawRGBBitmap(0, 0, image_data_JiJi, 128, 128); 
}

void displayTitle() {
    oled.drawRGBBitmap(0, 0, image_data_Logo, 128, 128); 
}

void displayGameOver() {
    oled.drawRGBBitmap(0, 0, image_data_Game_Over, 128, 128); 
}

  void drawGrid() {
    for (int x = 0; x < 10; x++) {
        for (int y = 0; y < 18; y++) {

            if (grid[x][y]) {
                oled.fillRect(
                    GRID_X + x * CELL,
                    GRID_Y + y * CELL,
                    CELL,
                    CELL,
                    WHITE
                );
            } else {
                oled.fillRect(
                    GRID_X + x * CELL,
                    GRID_Y + y * CELL,
                    CELL,
                    CELL,
                    BLACK
                );
            }
        }
    }
}

void erasePiece() {
    drawPiece(pieceX, pieceY, BLACK, piece);
}

void testdrawtext(char *text, uint16_t color) {
  oled.setCursor(0,0);
  oled.setTextColor(color);
  oled.print(text);
}

bool nextCollision() {
    for (int i = 0; i < 4; i++) {

        int x = pieceX + piece[0][i];
        int y = pieceY + piece[1][i] + 1;   // test one row below

        // bottom of playfield
        if (y > 17) return true;

        // ignore blocks above the visible grid
        if (y < 0) continue;

        // out of bounds horizontally (shouldn't really happen if other checks are good)
        if (x < 0 || x > 9) return true;

        // collision with existing block
        if (grid[x][y]) return true;
    }
    return false;
}

bool nextHorizontalCollision(const short testPiece[2][4], int amount) {
    for (int i = 0; i < 4; i++) {
        int newX = pieceX + testPiece[0][i] + amount;
        int newY = pieceY + testPiece[1][i];
        // Out of bounds
        if (newX < 0 || newX > 9)
            return true;

        // Ignore blocks above the screen
        if (newY < 0)
            continue;

        // Collision with grid
        if (grid[newX][newY])
            return true;
    }
    return false;
}
bool GameOver() {
    for (int i = 0; i < 4; i++) {
        int x = pieceX + piece[0][i];
        int y = pieceY + piece[1][i];

        // Only check blocks that are inside the visible grid
        if (y >= 0 && grid[x][y])
            return true;
    }
    return false;
}

void Tetris() {

    // --- GRAVITY / FALLING ---
    if (millis() - timer > interval) {

        checkLines();

        // 1. erase old
        erasePiece();

        // 2. try to move down
        if (!nextCollision()) {
            pieceY++;
            drawPiece(pieceX, pieceY, GREEN, piece);   // draw new
            
        } else {

            // 3. if still above grid, let it enter
            if (pieceY < 0) {
                pieceY++;
                drawPiece(pieceX, pieceY, GREEN, piece);
                return;
            }

            // 4. lock piece
            for (short i = 0; i < 4; i++)
                grid[pieceX + piece[0][i]][pieceY + piece[1][i]] = 1;

            drawGrid();
            generate();

            // Check for Game Over
            if (GameOver()) {
                displayGameOver();
                delay(1000);
                oled.fillScreen(BLACK);
                drawText("GAME OVER", 9, 30, 50);
                while (true);   // freeze the game
            }

            drawBlankNextPiece();
            drawNextPiece();
            return;
        }

        timer = millis();
    }

    // --- MOVE LEFT ---
    if (InData.b == true) {
        if (b1) {
            if (!nextHorizontalCollision(piece, -1)) {
                erasePiece();
                pieceX--;
                drawPiece(pieceX, pieceY, GREEN, piece);
            }
            b1 = false;
            InData.a = false;
        }
    } else {
        b1 = true;
    }

    // --- MOVE RIGHT ---
    if (InData.a == true) {
        if (b2) {
            if (!nextHorizontalCollision(piece, 1)) {
                erasePiece();
                pieceX++;
                drawPiece(pieceX, pieceY, GREEN, piece);
            }
            b2 = false;
            InData.b = false;
        }
    } else {
        b2 = true;
    }

    // --- SPEED DROP ---
    if (InData.c == true)
        interval = 20;
        //InData.b = false;
    else
        interval = 400;

    // --- ROTATION ---
    if (InData.d == true) {
        if (b3) {

            short newRot = (rotation + 1) % getMaxRotation(currentType);

            if (canRotate(newRot)) {
                erasePiece();
                rotation = newRot;
                copyPiece(piece, currentType, rotation);
                drawPiece(pieceX, pieceY, GREEN, piece);
            }

            b3 = false;
            InData.d = false;
            delayer = millis();
        }
    } else if (millis() - delayer > 50) {
        b3 = true;
    }
}

void Power_SW(){
    // has the button been pressed?
    if (Button_pin.isButtonPressed) {
        
        // yes! toggle display visibility
        isDisplayVisible = !isDisplayVisible;

        // apply
        oled.enableDisplay(isDisplayVisible);

        #if (SerialDebugging)
        Serial.print("button pressed @ ");
        Serial.print(millis());
        Serial.print(", display is now ");
        Serial.println((isDisplayVisible ? "ON" : "OFF"));
        #endif

        // confirm button handled
        Button_pin.isButtonPressed = false;
    }
}

void setup() {
   
    pinMode(Button_pin.PIN,INPUT_PULLUP);

    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
      Serial.println("Error initializing ESP-NOW");
      return;
    }
    esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

    // use an interrupt to sense when the button is pressed
    attachInterrupt(digitalPinToInterrupt(Button_pin.PIN), isr, FALLING);

    #if (SerialDebugging)
    Serial.begin(115200); while (!Serial); Serial.println();
    #endif

    // settling time
    delay(250);

    // ignore any power-on-reboot garbage
    Button_pin.isButtonPressed = false;

    // initialise the SSD1331
    oled.begin();
    oled.setFont();
    oled.setTextSize(1);
    displayJiJi();
    delay(3000);
    displayTitle();
    delay(3000);
    oled.fillScreen(BLACK);

    drawLayout(); 
    text_score();
    nextType = random(TYPES);
    generate();
    timer = millis();
    // the display is now on
    isDisplayVisible = true;
}

void loop() {
  //Power_SW();  
  Tetris();
}
