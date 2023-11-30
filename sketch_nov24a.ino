#include <SPI.h>                                // Knihovna pro komunikaci přes SPI
#include <MFRC522.h>                            // Knihovna pro RFID MFRC522
#include <ESP8266WiFi.h>                        // Knihovna pro WiFi funkce ESP8266
#include <WiFiManager.h>                        // Knihovna WiFiManager pro snadnou správu WiFi
#include <ESP8266WebServer.h>                   // Knihovna pro webový server na ESP8266
#include <NTPClient.h>                          // Knihovna pro práci s NTP (Network Time Protocol)
#include <WiFiUdp.h>                            // Knihovna pro UDP komunikaci přes WiFi
#include <time.h>                               // Knihovna pro práci s časem
#include <Wire.h>                               // Knihovna pro komunikaci přes I2C
#include <LiquidCrystal_I2C.h>                  // Knihovna pro práci s I2C LCD displeji
#include <ArduinoOTA.h>                         // Knihovna pro OTA (Over-the-Air) aktualizace
#include <ESP8266mDNS.h>                        // Knihovna pro podporu mDNS (multicast DNS)
#include <map>                                  // Knihovna pro použití datové struktury map

const int RST_PIN = D3;                         // Definice RST pinu pro RFID
const int SS_PIN = D4;                          // Definice SS pinu pro RFID
MFRC522 mfrc522(SS_PIN, RST_PIN);               // Vytvoření instance RFID čtečky
ESP8266WebServer server(80);                    // Vytvoření instance webového serveru na portu 80
WiFiUDP ntpUDP;                                 // Vytvoření instance UDP pro WiFi
NTPClient timeClient(ntpUDP);                   // Vytvoření instance NTP klienta

LiquidCrystal_I2C lcd(0x27, 16, 2);             // Inicializace LCD displeje s I2C adresou 0x27

int nextIdNo = 1;                               // Proměnná pro sledování dalšího ID registrace
void handleRoot();                              // Deklarace funkce handleRoot
void handleExportCSV();                         // Deklarace funkce handleExportCSV
void addRegistration(String cardID);            // Deklarace funkce pro přidání registrace

bool htmlSizePrinted = false;                   // Příznak pro sledování, zda byla velikost HTML kódu vypsána

// Struktura pro ukládání informací o registraci karet
struct CardRegistration {
  String idNo;
  String name;
  String cardID;
  String department;
  String date;
  String timeIn;
  String timeOut;
};

std::map<String, String> cardToNameMap;         // Mapa pro mapování ID karet na jména
std::vector<CardRegistration> registrations;    // Vektor pro ukládání registrací karet

// Funkce pro získání formátovaného data
String getFormattedDate() {
  time_t now = time(nullptr);                   // Získání aktuálního času
  struct tm* timeinfo = localtime(&now);        // Konverze času do lokálního času
  char buffer[20];                              // Buffer pro formátovaný řetězec
  strftime(buffer, 20, "%d.%m.%Y", timeinfo);   // Formátování data do řetězce
  return String(buffer);                        // Vrácení formátovaného data jako řetězce
}

// Funkce pro získání formátovaného času
String getFormattedTime() {
  time_t now = time(nullptr);                   // Získání aktuálního času
  struct tm* timeinfo = localtime(&now);        // Konverze času do lokálního času
  char buffer[20];                              // Buffer pro formátovaný řetězec
  strftime(buffer, 20, "%H:%M", timeinfo);      // Formátování času do řetězce
  return String(buffer);                        // Vrácení formátovaného času jako řetězce
}

void handleExportCSV() {
  String csv = "ID No;Name;Card ID;Department;Date;Time In;Time Out\n"; // Inicializace CSV řetězce s hlavičkami

  for (const auto& reg : registrations) {
    csv += reg.idNo + ";";                      // Přidání ID do CSV řetězce
    csv += reg.name + ";";                      // Přidání jména do CSV řetězce
    csv += reg.cardID + ";";                    // Přidání ID karty do CSV řetězce
    csv += reg.department + ";";                // Přidání oddělení do CSV řetězce
    csv += reg.date + ";";                      // Přidání data do CSV řetězce
    csv += reg.timeIn + ";";                    // Přidání času příchodu do CSV řetězce
    csv += reg.timeOut + "\n";                  // Přidání času odchodu a nového řádku pro každý záznam
  }

  server.send(200, "text/csv", csv);            // Odeslání CSV řetězce jako HTTP odpovědi
}

void connectToWiFi() {
  WiFiManager wifiManager;                      // Vytvoření instance WiFiManager
  //wifiManager.resetSettings();               // Resetování nastavení WiFi (zakomentováno)
  wifiManager.autoConnect("AutoConnectAP");     // Automatické připojení k WiFi
  Serial.println("Connected to WiFi network");  // Výpis stavu připojení na sériovou linku
  timeClient.begin();                           // Spuštění NTP klienta
}

void setup() {
  Serial.begin(115200);                         // Inicializace sériové komunikace s rychlostí 115200 baud
  SPI.begin();                                  // Inicializace SPI komunikace
  mfrc522.PCD_Init();                           // Inicializace RFID čtečky
  lcd.init();                                   // Inicializace LCD displeje
  lcd.backlight();                              // Zapnutí podsvícení LCD
  
  connectToWiFi();                              // Připojení k WiFi síti

  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // Nastavení časové zóny a NTP serverů
  while (!timeClient.update()) {
    timeClient.forceUpdate();                   // Vynucení aktualizace času, pokud není dosaženo
    delay(100);                                 // Krátké zpoždění mezi pokusy
  }

  if (MDNS.begin("esp8266")) {                  // Spuštění mDNS s hostname "esp8266"
    Serial.println("MDNS responder started");   // Výpis o spuštění mDNS na sériovou linku
  }

  server.on("/set-name", HTTP_GET, []() {       // Nastavení HTTP GET endpointu "/set-name"
    String cardID = server.arg("cardID");       // Získání parametru cardID z HTTP požadavku
    String userName = server.arg("name");       // Získání parametru name z HTTP požadavku
    setCardName(cardID, userName);              // Nastavení jména pro dané ID karty
    server.send(200, "text/plain", "Name set for card ID: " + cardID); // Odpověď serveru
  });

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";       // Aktualizace kódu
    } else { // U_SPIFFS
      type = "filesystem";   // Aktualizace souborového systému (SPIFFS)
    }
    // Poznámka: při aktualizaci SPIFFS by zde bylo místo pro odpojení SPIFFS pomocí SPIFFS.end()
    Serial.println("Start updating " + type);   // Výpis typu aktualizace na sériovou linku
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");     // Výpis o ukončení aktualizace
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));   // Výpis průběhu aktualizace
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);   // Výpis chyby, pokud nastane
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");    // Chyba autentizace
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");   // Chyba při zahájení aktualizace
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");    // Chyba připojení
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");   // Chyba při příjmu dat
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");       // Chyba při ukončení aktualizace
    }
  });

  ArduinoOTA.begin();                           // Zahájení OTA služby

 Serial.println("Time synchronized");           // Výpis o synchronizaci času
server.on("/", handleRoot);                    // Nastavení handleru pro kořenovou cestu
server.on("/export.csv", handleExportCSV);     // Nastavení handleru pro export CSV
server.begin();                                // Spuštění webového serveru
Serial.println("HTTP server started");         // Výpis o spuštění webového serveru
}

void setCardName(String cardID, String userName) {
  // Funkce pro nastavení jména k dané ID karty
  cardToNameMap[cardID] = userName; // Přidání nebo aktualizace jména v mapě
  Serial.println("Name set for card: " + cardID + " as " + userName); // Výpis o nastavení jména
}

void addRegistration(String cardID) {
  if (cardID.length() == 0) {
    Serial.println("Card ID is empty."); // Výpis, pokud je ID karty prázdné
    return;
  }

    timeClient.update();                         // Aktualizace času od NTP serveru
  String currentTime = getFormattedTime();    // Získání aktuálního času
  String currentDate = getFormattedDate();    // Získání aktuálního data

  String name = "Unknown";                     // Výchozí jméno, pokud není nalezeno
  if (cardToNameMap.find(cardID) != cardToNameMap.end()) {
    name = cardToNameMap[cardID];              // Použití jména z mapy, pokud je nalezeno
  }
  String department = "IAT";                   // Výchozí oddělení

  if (name != "") {
    lcd.clear();                               // Vyčištění LCD displeje
    lcd.setCursor(0, 0);                       // Nastavení kurzoru na začátek
    lcd.print("Card ID: " + cardID);           // Výpis ID karty na LCD
  }

 bool cardExists = false;
for (auto& reg : registrations) {
  if (reg.cardID.equalsIgnoreCase(cardID)) {
    reg.timeOut = currentTime; // Nastavení času odchodu, pokud karta existuje
    if (cardToNameMap.find(cardID) != cardToNameMap.end()) {
      reg.name = cardToNameMap[cardID]; // Aktualizace jména, pokud je v mapě
    }
    cardExists = true;
    break;
  }
}

  if (!cardExists) {
    CardRegistration newRegistration = {
      String(nextIdNo++),
      name,
      cardID,
      department,
      currentDate,
      currentTime,
      ""
    };
    registrations.push_back(newRegistration);
  }
}

  void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><title>Attendance</title></head><body>";
html += "<style>";
  html += "body { background-color: #e6ffe6; color: #333333; }";
  html += "h1 { text-align: center; }";
  html += "table { width: 100%; text-align: center; border-collapse: collapse; }";
  html += "th, td { border: 1px solid #000000; padding: 8px; }";
  html += "th { background-color: #4CAF50; color: white; }";
  html += "a { color: #1a73e8; text-decoration: none; }";
  html += "</style>";
  html += "</head><body>";
  html += "<ul>";
  html += "<li><a href='/'>Home</a></li>";
  html += "<li><a href='/export.csv'>Export </a></li>";
 
  html += "</ul>";
  html += "<meta http-equiv='refresh' content='5'></head><body>"; // Aktualizace každých 5 sekund
  html += "<h1>ATTENDANCE</h1>";
  html += "<table border='1' style='width:100%; text-align:center;'>"; // Střed textu pro celou tabulku
  html += "<tr><th>ID.No</th><th>Name</th><th>CardID</th><th>Department</th><th>Date</th><th>Time In</th><th>Time Out</th></tr>";

  for (const auto& reg : registrations) {
    html += "<tr>";
    html += "<td>" + reg.idNo + "</td>";
    html += "<td>" + reg.name + "</td>";
    html += "<td>" + reg.cardID + "</td>";
    html += "<td>" + reg.department + "</td>";
    html += "<td>" + reg.date + "</td>";
    html += "<td>" + reg.timeIn + "</td>";
    html += "<td>" + reg.timeOut + "</td>";
    html += "</tr>";
      }

 html += "</table></body></html>";                      // Ukončení HTML tabulky a dokumentu
if (!htmlSizePrinted) {                                // Kontrola, zda již byla vypsána velikost HTML kódu
    Serial.print("HTML code size: ");                  // Výpis velikosti HTML kódu na sériovou linku
    Serial.println(html.length());                     // Výpis délky HTML kódu
    htmlSizePrinted = true;                            // Nastavení příznaku, aby se velikost již nevypisovala
}
server.send(200, "text/html", html);                   // Odeslání HTML kódu klientovi

}

String readRFID() {
    String content = "";                               // Inicializace řetězce pro obsah RFID
    if (mfrc522.PICC_IsNewCardPresent() &&  mfrc522.PICC_ReadCardSerial()) {               // Kontrola, zda je přítomna nová karta a lze ji přečíst
        for (byte i = 0; i < mfrc522.uid.size; i++) {  // Iterace přes každý bajt UID karty
            if (mfrc522.uid.uidByte[i] < 0x10) {
                content += "0";                         // Přidání vedoucí nuly pro bajty menší než 16
            }
            content += String(mfrc522.uid.uidByte[i], HEX); // Přidání bajtu karty do řetězce ve formátu hex
        }
    content.toUpperCase();                             // Převedení obsahu na velká písmena
    }
    return content;                                    // Vrácení obsahu RFID karty
}

void loop() {
    ArduinoOTA.handle();                               // Zpracování OTA aktualizací
    server.handleClient();                             // Zpracování příchozích HTTP požadavků
    static unsigned long lastRFIDCheck = 0;            // Statická proměnná pro sledování času poslední kontroly RFID
    if (millis() - lastRFIDCheck > 5000) {             // Kontrola, zda uplynulo více než 5 sekund od poslední kontroly
        String cardID = readRFID();                    // Čtení RFID karty
        if (!cardID.isEmpty()) {                       // Kontrola, zda byla karta přečtena
            addRegistration(cardID);                   // Přidání registrace karty, pokud byla přečtena
        }
        lastRFIDCheck = millis();                      // Aktualizace času poslední kontroly RFID
    }
}