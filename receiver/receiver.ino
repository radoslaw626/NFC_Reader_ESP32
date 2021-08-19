HardwareSerial unit2(1);

void setup() {
  //(baud, data protocol, txd, rxd)
  unit2.begin(115200, SERIAL_8N1, 17, 16);
  Serial.begin(115200);

  Serial.println("Data received:");
}

void loop() {  
  while(unit2.available()) {
    char rxdChar = unit2.read();
    Serial.print(rxdChar);
  }
}
