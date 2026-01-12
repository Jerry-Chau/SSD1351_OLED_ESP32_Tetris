#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <esp_now.h>
#include <WiFi.h>

// put function declarations here:
using namespace std;

// Define Variables here:
const int AnalJoy_X = 33;
const int AnalJoy_Y = 32;
const int AnalJoy_SW = 26;

int potValueX;
int potValueY;
int potButtonSW;

// variable to check that the OLED receiver had acceppted the message
boolean RxLeft    = false;
boolean RxRight   = false;
boolean RxDrop    = false;
boolean RxRotate  = false;

int debounceDelay = 10;
int LastButtonState = 0;
int ButtonState = 0;
unsigned long lastDebounceTime = 0;

// REPLACE WITH YOUR RECEIVER MAC Address
uint8_t broadcastAddress[] = {0xa0, 0xb7, 0x65, 0x64, 0x60, 0xfc};

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  bool a;
  bool b;
  bool c;
  bool d;
} struct_message;

// Create a struct_message for sending Data
struct_message OutData;

esp_now_peer_info_t peerInfo;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.println();
  char macStr[18];
  Serial.print("Packet to: ");
  // Copies the sender mac address to a string
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print(macStr);
  Serial.print(" send status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");

  // If failed to deliver message then restart to accept serial messages
  if (status == 1){ 
    Serial.println("Please check that the receiver is functioning");         
  }
  Serial.println();
}









void Deliver_Message(){
  // Set values to send
  OutData.a = RxRight;
  OutData.b = RxLeft;
  OutData.c = RxDrop;
  OutData.d = RxRotate;

  // Send message via ESP-NOW
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &OutData, sizeof(OutData));

  if (result == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
}


void setup() {
  Serial.begin(115200);
  Serial.print("...Initializing...");

  pinMode(AnalJoy_X, INPUT);
  pinMode(AnalJoy_Y, INPUT);
  pinMode(AnalJoy_SW, INPUT_PULLUP);

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;

  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  
  delay(2000);
}


void loop() {
  // Reading potentiometer value
  potValueX = analogRead(AnalJoy_X);
  potValueY = analogRead(AnalJoy_Y);
  potButtonSW = digitalRead(AnalJoy_SW);

  //static char stringBuffer[64];
  //sprintf(stringBuffer, "\n\rX = %d, Y = %d, Button = %d", potValueX, potValueY, potButtonSW);
  //Serial.write(stringBuffer);
  Jstk_conv();
  delay(100);
}

void Jstk_conv (){
  if ( potValueX > (2000))
    RxRight = true;
  else if ( potValueX < (512+8))
    RxLeft = true;
  else{
      RxRight = false;
      RxLeft = false;
    }


  if ( potValueY < (420)){
    Serial.println("Drop");
    RxDrop = true;
  }
  else
    RxDrop = false;

  if (potButtonSW != LastButtonState) {
        lastDebounceTime = millis();
        RxRotate = false;
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay){
    if (potButtonSW != ButtonState) 
      ButtonState = potButtonSW;
      if (ButtonState == LOW) {
        RxRotate = true;
      }
    }
  LastButtonState = potButtonSW; 

  Deliver_Message();
}





  






