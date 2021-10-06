# arduino-lms-rfidremote
RFID based remote control for LMS based on Arduino

# Hardware
- Arduino MKR WiFi 1010
- Rotary Encoder KY-040
- RFID reader RC522
- Luxorparts LiPo 3.7V 1200 mAh battery

# Connections
- Rotary Encoder KY-040 -> MKR WiFi 1010
  - GND -> GND
  - \+ -> VCC
  - SW -> D1
  - DT -> D2
  - CLK -> D3
- RFID reader RC522 -> MKR WiFi 1010
  - SDA -> D7
  - RST -> D6
  - MISO -> MISO
  - SCK -> SCK
  - MOSI -> MOSI
  - 3.3V -> VCC
  - GND -> GND
