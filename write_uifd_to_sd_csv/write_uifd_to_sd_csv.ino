#include <SPI.h>
#include <SD.h>
#include <MFRC522.h>
#include <ErriezDS1302.h>
#include <RTClib.h>
#include <Wire.h>
#include <string>

// DS1302
const uint8_t DS1302_CLK_PIN = 14;
const uint8_t DS1302_IO_PIN = 12;
const uint8_t DS1302_CE_PIN = 13;

// MFRC522
const uint8_t MFRC522_RST_PIN = 4;
const uint8_t MFRC522_SS_PIN = 5;

// SD
const uint8_t SD_CS_PIN = 15;

// LED
const uint8_t LED_RED = 25;
const uint8_t LED_GREEN = 26;
const uint8_t LED_BLUE = 27;

ErriezDS1302 rtc = ErriezDS1302(DS1302_CLK_PIN, DS1302_IO_PIN, DS1302_CE_PIN);
MFRC522 rfid(MFRC522_SS_PIN, MFRC522_RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;
String NUID;
const String FILE_NAME = "db.csv";
File myFile;

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  
  // RC522 init
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  // Diodes init
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);

  Serial.println(F("This code scan the MIFARE Classsic NUID."));
  Serial.print(F("Using the following key: "));
  printHex(key.keyByte, MFRC522::MF_KEY_SIZE);

  // SD card init
  Serial.print("\r\nInitializing SD card...");
  SD.begin(SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("\r\nInitialization failed!");
    while (1);
  }
  Serial.println("\r\nInitialization done.");

  // Initialize I2C
  Wire.begin();
  Wire.setClock(100000);

  // Initialize RTC
  while (!rtc.begin()) {
    Serial.println(F("RTC not found"));
    delay(3000);
  }
  
  DateTime date = DateTime(F(__DATE__), F(__TIME__));
  if (!rtc.setDateTime(date.hour(), date.minute(), date.second(), date.day(), date.month(), date.year(), 0)) {
    Serial.println(F("Set date/time failed"));
  }
}

void loop() {
  if (readNUID()) {
    saveNUID();
  }
}

String readCurrentDateTime()
{
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint8_t mday;
  uint8_t mon;
  uint16_t year;
  uint8_t wday;

  // Read date/time
  if (!rtc.getDateTime(&hour, &min, &sec, &mday, &mon, &year, &wday)) {
    Serial.println(F("Read date/time failed"));
    return "Get date time error";
  }

  String secStr = String(sec);
  if (sec < 10) {
    secStr = "0" + String(sec);
  }

  String minStr = String(min);
  if (min < 10) {
    minStr = "0" + String(min);
  }

  String hourStr = String(hour);
  if (hour < 10) {
    hourStr = "0" + String(hour);
  }

  String mdayStr = String(mday);
  if (mday < 10) {
    mdayStr = "0" + String(mday);
  }

  String monStr = String(mon);
  if (mon < 10) {
    monStr = "0" + String(mon);
  }

  return hourStr + ":" + minStr + ":" + secStr + "-" + mdayStr + "-" + monStr + "-" + String(year);
}

int readNUID()
{
  // Look for new cards
  if ( ! rfid.PICC_IsNewCardPresent())
    return 0;

  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial())
    return 0;
    
  digitalWrite(LED_BLUE, LOW);
  
  Serial.print(F("\r\nPICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  Serial.println(rfid.PICC_GetTypeName(piccType));

  // Check is the PICC of Classic MIFARE type
  //  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
  //    piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
  //    piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
  //    Serial.println(F("Your tag is not of type MIFARE Classic."));
  //    return 0;
  //  }

  NUID = "";
  
  for (byte i = 0; i < rfid.uid.size; i++) {
    NUID.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
    NUID.concat(String(rfid.uid.uidByte[i], HEX));
  }
  
  NUID.remove(0, 1);
  NUID.toUpperCase(); 
  Serial.println("The NUID tag is: " + NUID);

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();

  return 1;
}

// Po wpieciu karty pierwszy odczyt ma puste NUID.
void saveNUID()
{
  // open the file, note that only one file can be open at a time,
  // so you have to close this one before opening another.
  myFile = SD.open("/" + FILE_NAME, FILE_APPEND);

  // if the file opened okay, write to it:
  if (myFile) {
    Serial.print("Writing to " + FILE_NAME + "...");
    String date = readCurrentDateTime();
    String csvData = String(NUID) + ", " + date + ";";
    myFile.println(csvData);

    // close the file:
    myFile.close();
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_GREEN, LOW);
    delay(1000);
    digitalWrite(LED_GREEN, HIGH); 
    Serial.println("\r\nSaved, file closed.");
  } else {
    // if the file didn't open, print an error:
    Serial.println("Error opening " + FILE_NAME);
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_RED, LOW); 
    delay(1000);
    digitalWrite(LED_RED, HIGH);
  }

  // re-open the file for reading:
  myFile = SD.open("/" + FILE_NAME);
  if (myFile) {
    Serial.println(FILE_NAME + ":");

    // read from the file until there's nothing else in it:
    while (myFile.available()) {
      Serial.write(myFile.read());
    }
    // close the file:
    myFile.close();
  } else {
    // if the file didn't open, print an error:
    Serial.println("Error opening " + FILE_NAME);
  }
}

/**
   Helper routine to dump a byte array as hex values to Serial.
*/
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/**
   Helper routine to dump a byte array as dec values to Serial.
*/
void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}
