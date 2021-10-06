#include <SPI.h>
#include <MFRC522.h>
#include <WiFiNINA.h>
#include <NoDelay.h>
#include <utility/wifi_drv.h>
#include "arduino_secrets.h"

// MFRC522 RFID related setup
#define SS_PIN 7
#define RST_PIN 6
MFRC522 rfid(SS_PIN, RST_PIN);

// KY-040 rotary encoder related setup
const int SW_PIN = 1;
const int DT_PIN = 2;
const int CLK_PIN = 3;

// Volume step since last volume adjustment
int volumeAdjustment = 0;

// Interval between RFID checks
noDelay rfidTime(500);
// Interval between volume adjustments
noDelay volumeAdjustmentTime(250);
// Interval between WiFi network connection checks
noDelay networkingTime(5000);


void setup()
{
  // Setup internal LED outputs
  WiFiDrv::pinMode(25, OUTPUT);
  WiFiDrv::pinMode(26, OUTPUT);
  WiFiDrv::pinMode(27, OUTPUT);

  // Setup KY-040 rotary controller inputs
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN, INPUT_PULLUP);
  pinMode(SW_PIN, INPUT_PULLUP);

  Serial.begin(9600);

  // Wait 30 seconds for serial to connect
  ledColor(25, 0, 0); // red
  while(!Serial && (millis() < 30000));

  // Establish WiFi connection
  ensureNetworkConnection();

  // Initialize RC522 device
  SPI.begin();
  rfid.PCD_Init(); // Init MFRC522 
  Serial.println("Please put the card to the induction area...");
  ledColor(0, 0, 0); // off
}

void loop()
{
  // Check for rotary encoder button
  int buttonClicked = checkButtonClicked();
  if(buttonClicked) {
    if(buttonClicked < 500 ) {
      triggerSongChange(1);
    }else if(buttonClicked < 1000) {
      triggerSongChange(-1);
    }else {
      triggerPlaybackPause();
    }
  }


  // Check for volume adjustment 
  int rotary = read_rotary();
  volumeAdjustment += rotary;
  if(volumeAdjustment != 0 && volumeAdjustmentTime.update()) {
    Serial.print("Change volume: ");
    Serial.println(volumeAdjustment);
    triggerVolumeChange(volumeAdjustment);
    volumeAdjustment = 0;
  }

  // Check for RFID card
  if(rfidTime.update()) {
    char cardId[9];
    if (getRFIDCard(cardId)) {
        ledColor(0, 25, 0); // green
        triggerPlaylist(cardId);
        delay(1000);
        ledColor(0, 0, 0); // off
    }
  }

  // Check WiFi connection and re-establish connection if not connected
  if (networkingTime.update()) {
    ensureNetworkConnection();
  }
}

// Check if RFID card is nearby
bool getRFIDCard(char * buf) {
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {

    for (int i = 0; i < 4; i++) {
      buf[i*2] = charToHex(0x0F & (rfid.uid.uidByte[i] >> 4));
      buf[i*2+1] = charToHex(0x0F & rfid.uid.uidByte[i]);
    }
    buf[8] = '\0';
    Serial.print("New card detected: ");
    Serial.println(buf);

    // Halt PICC
    rfid.PICC_HaltA();
  
    // Stop encryption on PCD
    rfid.PCD_StopCrypto1();     
    return true; 
  }
  return false;
}

// Check if button has been clicked and in that way for how long time
int lastButtonPushed = LOW;
int buttonPushedMillis = 0;
int checkButtonClicked() {
  int pushTime = 0;
  int buttonPushed = !digitalRead(SW_PIN);
  if (buttonPushed && !lastButtonPushed) {
    buttonPushedMillis = millis();
    delay(10);
  }else if(!buttonPushed && lastButtonPushed) {
    pushTime = millis() - buttonPushedMillis;
    delay(10);
  }
  lastButtonPushed = buttonPushed;
  return pushTime;
}


// Ensure network is connected and connect if it isn't
int wifiStatus = WL_IDLE_STATUS;
char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;
void ensureNetworkConnection() {
  wifiStatus = WiFi.status();
  if( wifiStatus != WL_CONNECTED ) {
    ledColor(0, 0, 25); // blue
    while (wifiStatus != WL_CONNECTED) {
      Serial.print("Attempting to connect to network:");
      Serial.println(ssid);
      wifiStatus = WiFi.begin(ssid, pass);
      WiFi.lowPowerMode();
  
      int i=0;
      while (wifiStatus != WL_CONNECTED && i<10) {
        delay(1000);
        wifiStatus = WiFi.status();
        i++;
      }
    }
    Serial.print("You're connected to the network: ");
    Serial.print(WiFi.SSID());
    Serial.print(" with IP: ");
    Serial.println(WiFi.localIP());
    ledColor(0, 0, 0); // off
  }
}

// Issue volume change request
void triggerVolumeChange(int volumeAdjustment) {
  String change = String("");
  if(volumeAdjustment>0) {
    change = String("+");
  }
  change = change + volumeAdjustment * 2;

  httpGet(String("/plugins/RFIDPlay/volume/") + change);
}

// Issue song change request
void triggerSongChange(int songChange) {
  String change = String("");
  if(songChange>0) {
    change = String("+");
  }
  change = change + songChange;

  httpGet(String("/plugins/RFIDPlay/playlist/") + change);
}

// Issue player to switch pause state
void triggerPlaybackPause() {
  httpGet("/plugins/RFIDPlay/playlist/pause");
}

// Issue play request
void triggerPlaylist(String cardID) {
  httpGet(String("/plugins/RFIDPlay/play/") + cardID);
}

// Make HTTP request
char lmsIp[] = SECRET_LMS_IP;
void httpGet(String path) {
  WiFiClient client;
  int port = 9000;
  client.stop();
  if (client.connect(lmsIp,port)) {
    Serial.println(String("Connected to: ") + lmsIp);
    String message = String("GET") + " " + path + " HTTP/1.1" + "\r\n" + 
      "Host: " + lmsIp +":"+port + "\r\n" +
      "\r\n";
    Serial.println(message);
    
    client.print(message);
    client.stop();
  }
      
}

// Read rotation state of rotary controller
// A vald CW move returns 1, CCW move returns -1, invalid or no move returns 0.
static uint8_t prevNextCode = 0;
static uint16_t store=0;
int8_t read_rotary() {
  static int8_t rot_enc_table[] = {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0};

  prevNextCode <<= 2;
  if (digitalRead(DT_PIN)) prevNextCode |= 0x02;
  if (digitalRead(CLK_PIN)) prevNextCode |= 0x01;
  prevNextCode &= 0x0f;

   // If valid then store as 16 bit data.
   if  (rot_enc_table[prevNextCode] ) {
      store <<= 4;
      store |= prevNextCode;
      if ((store&0xff)==0x2b) return -1;
      if ((store&0xff)==0x17) return 1;
   }
   return 0;
}

// Activate or deactive the internal let with a specific RGB color
void ledColor(int red, int green, int blue) {
  WiFiDrv::analogWrite(25, red);  //RED
  WiFiDrv::analogWrite(26, green);   //GREEN
  WiFiDrv::analogWrite(27, blue);   //BLUE
}

// Convert single digit number to hex char
char charToHex(char c)
{  
  return "0123456789abcdef"[0x0F & (unsigned char)c];
}
