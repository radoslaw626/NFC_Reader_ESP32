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
MFRC522 rfid(MFRC522_SS_PIN, MFRC522_RST_PIN);
String NUID;
const String FILE_NAME = "db.csv";
File myFile;
HardwareSerial unit1(1);
uint32_t chipId;
bool newData = false;

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // Diodes init
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);

  // MFRC522 init
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522

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

  //#################### Bledy: ##########################
  // Opoznienie miedzy pobraniem czasu a wgraniem na rtc

  // Synchronise RTC with PC date and time
  DateTime date = DateTime(F(__DATE__), F(__TIME__));
  if (!rtc.setDateTime(date.hour(), date.minute(), date.second(), date.day(), date.month(), date.year(), 0)) {
    Serial.println(F("Set date/time failed"));
  }

  // Connection with second ESP
  //(baud, data protocol, txd, rxd)
  unit1.begin(115200, SERIAL_8N1, 17, 16);

  readUniqueId();
}

void loop() {
  // send new data if devices are connected
  bool isConnected = true;
  if (isConnected) {
    if (newData) {
      sendData();
    }
    return;
  }

  if (readNUID()) {
    saveNUID();
  }
}

void readUniqueId() {
  for (int i = 0; i < 17; i = i + 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }

  Serial.print("Chip ID: ");
  Serial.println(chipId);
}

void sendData() {
  // open the file for reading:
  myFile = SD.open("/" + FILE_NAME);
  if (myFile) {
    // read from the file until there's nothing else in it
    while (myFile.available()) {
      unit1.write(myFile.read());
    }

    myFile.close();
    Serial.println("Data sent");

    SD.remove("/" + FILE_NAME);
    newData = false;
    Serial.println("File deleted");
  }
  else {
    // if the file didn't open, print an error:
    Serial.println("Error opening " + FILE_NAME);
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

  // remove empty whitespace
  NUID.remove(0, 1);
  NUID.toUpperCase();
  Serial.println("The NUID tag is: " + NUID);

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();

  return 1;
}

//#################### Bledy: ##########################
// Po wpieciu karty pierwszy odczyt ma puste NUID.
// Po wypieciu karty nastepny zapis bedzie mial zmienna file i "zapisuje" na karte ktora nie jest wpieta
void saveNUID()
{
  // open the file, note that only one file can be open at a time,
  // so you have to close this one before opening another.
  myFile = SD.open("/" + FILE_NAME, FILE_APPEND);

  // if the file opened okay, write to it:
  if (myFile) {
    Serial.println("Writing to " + FILE_NAME + "...");
    String csvData = String(chipId) + "," + String(NUID) + "," + readCurrentDateTime() + ";";
    byte bytesWritten = myFile.println(csvData);
    newData = true;
    Serial.println("Bytes written: " + String(bytesWritten));

    // close the file:
    myFile.close();
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_GREEN, LOW);
    delay(1000);
    digitalWrite(LED_GREEN, HIGH);
    Serial.println("Saved, file closed.");
  } else {
    // if the file didn't open, print an error:
    Serial.println("Error opening " + FILE_NAME);
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_RED, LOW);
    delay(1000);
    digitalWrite(LED_RED, HIGH);
  }
}
