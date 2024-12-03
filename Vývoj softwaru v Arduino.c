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
#include <map>                                  // Knihovna pro použití datové struktury map
#include <LittleFS.h>
#include <FTPClient.h>
#include <FTPServer.h>

const int RST_PIN = D3;                         // Definice RST pinu pro RFID
const int SS_PIN = D4;                          // Definice SS pinu pro RFID
MFRC522 mfrc522(SS_PIN, RST_PIN);               // Vytvoření instance RFID čtečky
ESP8266WebServer server(80);                    // Vytvoření instance webového serveru na portu 80
WiFiUDP ntpUDP;                                 // Vytvoření instance UDP pro WiFi
NTPClient timeClient(ntpUDP);                   // Vytvoření instance NTP klienta

LiquidCrystal_I2C lcd(0x27, 16, 2);             // Inicializace LCD displeje s I2C adresou 0x27

const char* ftp_server = "193.169.45.7"; // Your computer's IP address
const char* ftp_username = "android"; // FTP server username
const char* ftp_password = "android"; // FTP server password
const int ftp_port = 2221; // FTP server port, usually 21 if not changed

// Declaring ftpClient in global scope
FTPClient ftpClient(LittleFS);
FTPClient::ServerInfo ftpServerInfo(ftp_username, ftp_password, ftp_server, ftp_port);

int nextIdNo = 1;                               // Proměnná pro sledování dalšího ID registrace
void handleRoot();                              // Deklarace funkce handleRoot
void handleExportCSV();                         // Deklarace funkce handleExportCSV
void addRegistration(String cardID);            // Deklarace funkce pro přidání registrace

bool htmlSizePrinted = false;                   // Příznak pro sledování, zda byla velikost HTML kódu vypsána

// Global variables
unsigned long startTime;
bool transferStarted;
String html; 

// Struktura pro ukládání informací o registraci karet
struct CardRegistration {
  String idNo;
  String name;
  String cardID;
  String department;
  String date;
  String timeIn;
  String timeOut;
  bool lastScanWasExit; // New field to track last event
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

void connectToWiFiAndStartServer() {
  WiFiManager wifiManager;                      // Vytvoření instance WiFiManager
   //wifiManager.resetSettings();               // Resetování nastavení WiFi (zakomentováno)
  
if (!wifiManager.autoConnect("AutoConnectAP")) {
    Serial.println("Failed to connect to WiFi. Entering Access Point mode.");
    // Setup AP mode
    WiFi.softAP("ESP8266-AP", "password");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Connected to WiFi network!");
    // Setup regular mode
    WiFi.begin(); 
  }
}

// This function declaration suggests that there is a function that saves HTML content to a file, probably on the ESP8266's filesystem.
// The function takes a string containing HTML and returns a boolean value indicating success or failure of the operation.
bool saveHTMLToFile(const String& htmlContent);

// This function declaration suggests that there is a function that handles the exporting of a file to an FTP server.
bool exportHTMLtoFTP();

// This function is a route handler for a web server request that likely initiates an export process. When this route is accessed, it sends a response with a status code of 200 (OK) and a plain text message "Export started".
void handleExportRequest() {
    server.send(200, "text/plain", "Export started"); // Respond with a plain text message
}

void setup() {
  Serial.begin(115200);                         // Inicializace sériové komunikace s rychlostí 115200 baud
  delay(500);
    ftpClient.begin(ftpServerInfo);

    Serial.println(F("Inizializing FS..."));
  if (LittleFS.begin()) {
    Serial.println(F("done."));
} else {
    Serial.println(F("fail."));
}
  // Get all information of your LittleFS
    FSInfo fs_info;
    LittleFS.info(fs_info);
 
    Serial.println("File sistem info.");
 
    Serial.print("Total space:      ");
    Serial.print(fs_info.totalBytes);
    Serial.println("byte");
 
    Serial.print("Total space used: ");
    Serial.print(fs_info.usedBytes);
    Serial.println("byte");
 
    Serial.print("Block size:       ");
    Serial.print(fs_info.blockSize);
    Serial.println("byte");
 
    Serial.print("Page size:        ");
    Serial.print(fs_info.totalBytes);
    Serial.println("byte");
 
    Serial.print("Max open files:   ");
    Serial.println(fs_info.maxOpenFiles);
 
    Serial.print("Max path lenght:  ");
    Serial.println(fs_info.maxPathLength);
 
    Serial.println();
 
    // Open dir folder
    Dir dir = LittleFS.openDir("/");
    // Cycle all the content
    while (dir.next()) {
        // get filename
        Serial.print(dir.fileName());
        Serial.print(" - ");
        // If element have a size display It else write 0
        if(dir.fileSize()) {
            File f = dir.openFile("r");
            Serial.println(f.size());
            f.close();
        }else{
            Serial.println("0");
        }
    }

  File file = LittleFS.open("/export.html", "r");
  if (!file) {
    Serial.println("Failed to open file export.html");
    return;
  }

  Serial.println("File contents export.html:");
  while (file.available()) {
    Serial.write(file.read());
  }
  file.close();

  delay(5000);

  SPI.begin();                                  // Inicializace SPI komunikace
  mfrc522.PCD_Init();                           // Inicializace RFID čtečky
  lcd.init();                                   // Inicializace LCD displeje
  lcd.backlight();                              // Zapnutí podsvícení LCD
  
  connectToWiFiAndStartServer();                              // Připojení k WiFi síti

  configTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov"); // Nastavení časové zóny a NTP serverů
  while (!timeClient.update()) {
    timeClient.forceUpdate();                   // Vynucení aktualizace času, pokud není dosaženo
    delay(100);                                 // Krátké zpoždění mezi pokusy
  }

 // Set up your server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/export.csv", HTTP_GET, handleExportCSV);
  server.on("/export", HTTP_GET, handleExport); // Ensure handleExport is defined before setup()

  // Start the server
  server.begin();
  Serial.println("HTTP server started.");

  server.on("/set-name", HTTP_GET, []() {       // Nastavení HTTP GET endpointu "/set-name"
    String cardID = server.arg("cardID");       // Získání parametru cardID z HTTP požadavku
    String userName = server.arg("name");       // Získání parametru name z HTTP požadavku
    setCardName(cardID, userName);              // Nastavení jména pro dané ID karty
    server.send(200, "text/plain", "Name set for card ID: " + cardID); // Odpověď serveru
  });
  // Обработчик для загрузки данных
server.on("/download-data", HTTP_GET, []() {
  File dataFile = LittleFS.open("/data.csv", "r");  // Open the file for reading
  if (!dataFile) {
    server.send(500, "text/plain", "Failed to open data file");
    return;
  }
  server.streamFile(dataFile, "application/octet-stream"); // Send the file content to the client
  dataFile.close();
});

 Serial.println("Time synchronized");           // Výpis o synchronizaci času
}

void saveDataToFile(String cardID) {
  File file = LittleFS.open("/data.csv", "a"); // Открыть файл для добавления данных
  if (!file) {
    Serial.println("Failed to open file for appending");
    return;
  }
  String data = cardID + "," + getFormattedDate() + " " + getFormattedTime() + "\n";
  file.print(data);
  file.close();
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
if (reg.cardID.equals(cardID))  {
      if (reg.lastScanWasExit) {
   // If the last scan was an exit, update the entry time.
         reg.timeIn = currentTime;
         reg.lastScanWasExit = false; // Set the last event flag as input
       } else {
         // If the last scan was an input, update the output time.
         reg.timeOut = currentTime;
         reg.lastScanWasExit = true; // Set the last event flag as output
      }
      cardExists = true;
      break;
    }
  }

// If the card has not been scanned before, create a new registration.
  if (!cardExists) {
    CardRegistration newRegistration = {
      String(nextIdNo++), // idNo
      name,               // name
      cardID,             // cardID
      department,         // department
      currentDate,        // date
      currentTime,        // timeIn
      "",                 // timeOut
      false               // lastScanWasExit
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
  html += "<li><a href='/export'>Export HTML to FTP</a></li>";
 
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
     if (saveHTMLToFile(html)) {
    // Handle successful save
  } else {
    // Handle save error
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

String generateCSVData() {
  String csv = "ID No;Name;Card ID;Department;Date;Time In;Time Out\n";
  for (const auto& reg : registrations) {
    csv += reg.idNo + ";";
    csv += reg.name + ";";
    csv += reg.cardID + ";";
    csv += reg.department + ";";
    csv += reg.date + ";";
    csv += reg.timeIn + ";";
    csv += reg.timeOut + "\n";
  }
  return csv;
}

bool saveCSVToFile(const String& path, const String& data) {
  File file = LittleFS.open(path, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return false;
  }
  if (file.print(data)) {
    file.close();
    return true;
  } else {
    file.close();
    return false;
  }
}

void listDir(String indent, String path) {
    Dir dir = LittleFS.openDir(path); // Open the directory at the given path
    while (dir.next()) { // While there are files or directories in the current directory
        File entry = dir.openFile("r"); // Open the current file or directory
        if (entry.isDirectory()) { // Check if it's a directory
            Serial.println(indent + "  Dir: " + dir.fileName()); // Print directory name
            listDir(indent + "  ", path + dir.fileName() + "/"); // Recursively list contents
        } else {
            // It's a file, print its name and size
            Serial.println(indent + "File: " + dir.fileName() + "\tSize: " + String(entry.size()));
        }
        entry.close(); // Close the file to free up resources
    }
}

void getFileFromFTP(String fileName) {
    Serial.println("Starting to download a file from FTP: " + fileName); // Inform the user
    // Attempt to start the file transfer from the FTP server to the local path
    FTPClient::Status status = ftpClient.transfer(fileName, "/local/path/" + fileName, FTPClient::FTP_GET_NONBLOCKING);
    if (status.result == FTPClient::OK) {
        // If the transfer initiation was successful, inform the user
        Serial.println("File download initiated.");
    } else {
        // If there was an error, print the error description
        Serial.println("Error initiating file download: " + status.desc);
    }
}

void startFTPUpload(const String& localFile, const String& remoteFile) {
   Serial.printf("Starting uploading file %s to FTP...\n", localFile.c_str());
     FTPClient::Status status = ftpClient.transfer(localFile, remoteFile, FTPClient::FTP_PUT_NONBLOCKING);
   if (status.result == FTPClient::OK) {
     Serial.println("File transfer initiated.");
     transferStarted = true; // Set this flag to true to indicate transfer has started
   } else {
     Serial.printf("Error initiating file transfer: %s\n", status.desc.c_str());
     transferStarted = false; // Set or reset this flag as appropriate
   }
}

 void loop(){
    server.handleClient();                             // Zpracování příchozích HTTP požadavků
    ftpClient.handleFTP();
    static unsigned long lastRFIDCheck = 0;            // Statická proměnná pro sledování času poslední kontroly RFID
    if (Serial.available() > 0) {
        String command = Serial.readStringUntil('\n');
        command.trim(); // Remove any whitespace or newline characters
        if (command == "L") {
            // List the contents of the file system
            listDir("", "/");
        } else if (command.startsWith("G ")) {
            // Get a file from the FTP server
            String fileName = command.substring(2); // Extract filename
            getFileFromFTP(fileName);
        }
          }
        if (transferStarted) {                         // Check file transfer status if transfer has been initiated
     FTPClient::Status status = ftpClient.check();
     if (status.result == FTPClient::OK) {
       Serial.println("File transfer completed successfully.");
       transferStarted = false; // Reset the transmission start flag
     } else if (status.result == FTPClient::ERROR) {
       Serial.printf("File transfer error: %s\n", status.desc.c_str());
       transferStarted = false; // Reset the transmission start flag
    }
  }
    if (millis() - lastRFIDCheck > 5000) {             // Kontrola, zda uplynulo více než 5 sekund od poslední kontroly
        String cardID = readRFID();                    // Čtení RFID karty
        if (!cardID.isEmpty()) {                       // Kontrola, zda byla karta přečtena
            addRegistration(cardID);                   // Přidání registrace karty, pokud byla přečtena
                    }
        lastRFIDCheck = millis();                      // Aktualizace času poslední kontroly RFID
    }
}

// This function tries to save a given HTML content string to a file.
// It returns true if successful, or false if it fails.
bool saveHTMLToFile(const String& htmlContent) {
    Serial.println("Saving HTML to file...");  // Indicate the start of the save operation in the serial output for debugging.
     File file = LittleFS.open("/export.html", "w");   // Try to open (or create if it doesn't exist) a file named "/export.html" for writing.

  if (!file) {       // Check if the file was successfully opened.
       Serial.println("Failed to open file for writing");    // If the file couldn't be opened, print an error message to the serial output.
    return false;      // Return false to indicate failure.
  }
  if (file.print(htmlContent)) {        // Write the HTML content to the file.
    Serial.println("File was written");      // If writing was successful, print a confirmation message.
    file.close();       // Close the file to ensure data is written to the filesystem.
    return true;        // Return true to indicate success
  } else {
    Serial.println("File write failed");   // If writing failed, print an error message.
    file.close();      // Close the file to release the created object.
    return false;     // Return false to indicate failure.
  }
}

bool exportHTMLtoFTP() {
  Serial.println("Starting FTP transfer...");
  ftpClient.transfer("/export.html", "/remote/path/export.html", FTPClient::FTP_PUT_NONBLOCKING); // Start the FTP transfer
  uint32_t startTime = millis();        // Check the status of the FTP transfer
  while (true) {
    ftpClient.handleFTP();  // Regularly call handleFTP
    FTPClient::Status status = ftpClient.check();
        // Check for FTP transfer status
    if (status.result == FTPClient::OK || status.result == FTPClient::ERROR) {
      Serial.print("FTP Response Code: ");
      Serial.println(status.code);
      Serial.print("FTP Response Description: ");
      Serial.println(status.desc);
      
      if (status.result == FTPClient::OK) {
        Serial.println("FTP Transfer complete.");
        return true;
      } else {
        Serial.println("FTP Transfer failed.");
        return false;
      }
    }
    if (millis() - startTime > 30000) { // Timeout after 30 seconds
      Serial.println("FTP transfer timeout.");
      return false;
    }
    delay(100); // Small delay to prevent blocking the loop
  }
}

bool exportFileToFTP(const String& localPath, const String& remotePath) {
  Serial.println("Starting FTP transfer...");
  ftpClient.transfer(localPath, remotePath, FTPClient::FTP_PUT_NONBLOCKING);
  uint32_t startTime = millis();
  while (true) {
    ftpClient.handleFTP();
    FTPClient::Status status = ftpClient.check();
    if (status.result == FTPClient::OK || status.result == FTPClient::ERROR) {
      if (status.result == FTPClient::OK) {
        Serial.println("FTP Transfer complete.");
        return true;
      } else {
        Serial.println("FTP Transfer failed: " + status.desc);
        return false;
      }
    }
    if (millis() - startTime > 30000) {
      Serial.println("FTP transfer timeout.");
      return false;
    }
    delay(100);
  }
}

void handleExport() {
  // First, generate the CSV data and save it to a file
  String csvData = generateCSVData(); // Implement this function to generate CSV data
  saveCSVToFile("/export.csv", csvData); // Implement this function to save CSV data to file

  // Now attempt to export the file to FTP
  bool exported = exportFileToFTP("/export.csv", "/remote/path/export.csv"); // Modify this function to accept file paths

  // Send response based on whether the export was successful
  if (exported) {
    server.send(200, "text/plain", "Export to FTP successful.");
    Serial.println("Export to FTP successful.");
  } else {
    server.send(500, "text/plain", "Export to FTP failed.");
    Serial.println("Export to FTP failed.");
  }
}    
