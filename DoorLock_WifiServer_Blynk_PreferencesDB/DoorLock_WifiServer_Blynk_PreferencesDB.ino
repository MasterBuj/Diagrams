// FINAL WORKIG KEYPIN AND RFID
//  WORKING PIN, OTP, RFID, PARTY MODE
//  TODO: Send log


// important includes

// BLYNK Config
#define BLYNK_TEMPLATE_ID "TMPL4XF9eM7W"
#define BLYNK_DEVICE_NAME "SMART DOOR LOCK"
#define BLYNK_AUTH_TOKEN "rCw6B7YzfCspsMBTO1x85GbJaDdwLq47"
#define VPIN_BUTTON_0 V0
#include <BlynkSimpleEsp32.h>
char auth[] = BLYNK_AUTH_TOKEN;
BlynkTimer timer;


// WIFI SERVER config
#include <WiFi.h>
#include <WebServer.h>
WebServer server(80);
// Set your Static IP address
IPAddress local_IP(192, 168, 1, 184);
IPAddress gateway(192, 168, 1, 8);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);    //optional
IPAddress secondaryDNS(8, 8, 4, 4);  //optional

// DB config
#include <Preferences.h>
Preferences DB;


// RFID SCANNER Config
#include <SPI.h>
#include <MFRC522.h>
#define RST_PIN 22
#define SS_PIN 21
MFRC522 mfrc522(SS_PIN, RST_PIN);  //MFRC522 instance


// KEYPAD Config
#include <Keypad.h>
const byte numRows = 4;  //number of rows on the keypad
const byte numCols = 4;  //number of columns on the keypad
char keymap[numRows][numCols] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
byte rowPins[numRows] = { 13, 12, 14, 27 };
byte colPins[numCols] = { 26, 25, 33, 32 };
Keypad myKeypad = Keypad(makeKeymap(keymap), rowPins, colPins, numRows, numCols);

// GPIOs
#define R1_LED 4
#define R2_LED 15
#define G_LED 2
#define Lock_LED 5

// variables will use
// bool otpUsed = false;
bool RFIDMode = false;
String tagUID = "0754f5a6";
String pinCode = "6601";  //The default pinCode
String visitorInputPin;   //  store otp input
// bool factoryMode = true;
bool doorStatus = false;
bool isPartyMode = false;

bool modePrintRFID = true;
bool modePrintPIN = true;
char keypressed;

String tagsToSend = " ";
bool startTimer = false;

int wifiMode = 0;
bool isServerOn = false;
bool isBlynkOnRun = false;
bool isWifiServerOnRun = false;
String ssid;
String password;
// wifi mode 0=wifi   1=AP

TaskHandle_t Task1;


// Opening door on thread 1 priority 1
void Task1code(void* pvParameters) {
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;) {
    delay(10);
    if (doorStatus) {
      digitalWrite(G_LED, HIGH);
      digitalWrite(Lock_LED, HIGH);

      printthis("DOOR OPEN!");
      delay(3000);
      printthis("DOOR CLOSE!");

      digitalWrite(G_LED, LOW);
      digitalWrite(Lock_LED, LOW);
      doorStatus = false;
      modePrintPIN = true;
    }

    if (startTimer) {
      byte sec = 15;
      for (byte i = 0; i < sec && startTimer == true; i++) {
        delay(1000);
        // Serial.println(i);
      }
      startTimer = false;
    }
  }
}

void printConfig() {
  Serial.println("\n============CONFIG=============");

  Serial.println("PINCODE: " + pinCode);
  Serial.println("TAG: " + tagUID);
  Serial.println("CONNECTION TYPE: " + String((wifiMode == 1) ? "Acces Point" : (wifiMode == 2) ? "WiFi"
                                                                                                : "Offline"));
  Serial.println("SERVER: " + String((isServerOn) ? "Online" : "Offline"));
  Serial.println("SERVER TYPE: " + String((isBlynkOnRun) ? "Blynk" : "Wifi Server"));

  Serial.println("===========END OF CONFIG=========");
}

void wifiConn(String ssid, String password) {


  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  delay(1000);

  // Configures static IP address
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }

  Serial.print("Setting WIFI ");
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.print("Connecting to ");
  Serial.println(ssid);

  int z = 0;
  while (WiFi.status() != WL_CONNECTED) {
    z++;
    Serial.print(".");
    delay(1000);
    if (z >= 10) {
      DB.begin("Credentials", false);
      int counter = DB.getInt("counter", 0);
      Serial.printf("Restart counter value: %u\n", counter);
      counter++;
      DB.putInt("counter", counter);
      if (counter >= 3) {
        //cant connect
        DB.putString("SSID", "");
        DB.putString("Password", "");
      }
      DB.end();
      Serial.println("");
      ESP.restart();
    }
  }

  Serial.println("WiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Local IP address: ");
  Serial.println(WiFi.localIP());
}

void wifiAP() {
  // Configures static IP address

  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_AP);
  delay(1000);

  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }

  Serial.print("Setting AP (Access Point)…");
  WiFi.softAP("DOORLOCK", "DOORLOCK", 1, false, 1);  //WiFi.softAP(ssid, passphrase, channel, ssdi_hidden, max_connection)
  Serial.println("SSID:DOORLOCK :: PSK:DOORLOCK");

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  Serial.print("Local IP address: ");
  Serial.println(WiFi.localIP());
}


void setup() {

  Serial.begin(9600);

  pinMode(R1_LED, OUTPUT);
  pinMode(R2_LED, OUTPUT);
  pinMode(G_LED, OUTPUT);
  pinMode(Lock_LED, OUTPUT);
  delay(3000);

  xTaskCreatePinnedToCore(
    Task1code, /* Task function. */
    "Task1",   /* name of task. */
    10000,     /* Stack size of task */
    NULL,      /* parameter of the task */
    0,         /* priority of the task */
    &Task1,    /* Task handle to keep track of created task */
    0);        /* pin task to core 0 */


  delay(500);

  Serial.println("\n============CONFIG============");

  DB.begin("Credentials", false);
  if (DB.getString("pincode", "") == "") {
    DB.putString("pincode", pinCode);
  } else {
    pinCode = DB.getString("pincode", "");
  }

  DB.end();


  Serial.println("Pincode: " + pinCode);
  Serial.println("Tag: " + tagUID);
  Serial.println("Connection Type: " + String((wifiMode == 1) ? "Acces Point" : (wifiMode == 2) ? "WiFi"
                                                                                                : "Offline"));
  Serial.println("Server: " + String((isServerOn) ? "Online" : "Offline"));
  Serial.println("SERVER TYPE: " + String((isBlynkOnRun) ? "Blynk" : "Wifi Server"));

  SPI.begin();         // Init SPI bus
  mfrc522.PCD_Init();  // Init MFRC522
  Blynk.config(auth);  // Init Blynk

  Serial.println("===========END OF CONFIG=========");
  blink(5, 0);
}

void loop() {
  allLedOff();

  if (isWifiServerOnRun) { server.handleClient(); }
  if (isBlynkOnRun) { Blynk.run(); }
  // PIN MODE
  if (RFIDMode == false && isPartyMode == false) {

    if (modePrintPIN == true) {
      Serial.println("\n===== MENU ===== ");
      Serial.println("PIN mode \nPress * to Enter PIN \nPress A to Change PIN \nPress B to Add RFID Tag \nPress D to Start Server");
      // if (factoryMode) {
      //   Serial.println("FACTORY MODE: RFIDs [0], VISITORS PIN [0], WIFI [0] ");
      //   Serial.println("Default pincode: " + pinCode);
      // }
      Serial.println("==================");
      modePrintPIN = false;
    }

    keypressed = myKeypad.getKey();
    // Input pinCode
    if (keypressed == '*') {
      allLedOff();
      digitalWrite(R1_LED, HIGH);
      Serial.println("Enter pinCode");
      Serial.println("Current pincode " + pinCode);
      String inputPIN = GET_PINCODE();

      if (pinCode == inputPIN) {
        RFIDMode = true;
      } else if (IS_IN_DB(inputPIN, "pin")) {  //check if visitors pin is used

        allLedOff();
        digitalWrite(G_LED, HIGH);

        OpenDoor();  //Open lock function if code is correct

        digitalWrite(G_LED, LOW);
        modePrintPIN = true;
      } else {
        digitalWrite(R1_LED, LOW);
        printthis("Untegistered pin trying to accesses your door lock");
        delay(1000);
        modePrintPIN = true;
      }
      modePrintPIN = true;
    }

    // CHANGE PIN CODE
    if (keypressed == 'A') {
      CHANGE_PINCODE();
      modePrintPIN = true;
    }  // END of change pincode

    // ADD RFID TAG
    if (keypressed == 'B') {
      allLedOff();
      digitalWrite(R1_LED, HIGH);
      Serial.println("Scan your RFID to register! ");
      String tag1 = SCAN_TAG();
      if (tag1 == "") {
        RFIDMode = false;
        modePrintPIN = true;
        return;
      }
      Serial.println(tag1);
      delay(1000);
      digitalWrite(R2_LED, HIGH);
      Serial.println("[Info] Scan again to veritfy");
      String tag2 = SCAN_TAG();
      Serial.println(tag2);

      if (tag1 == tag2) {
        allLedOff();
        digitalWrite(G_LED, HIGH);
        Serial.println("[Info] tag match");
        Serial.println("Registering " + tag1 + " tag ID");

        DB.begin("Credentials", false);
        // tagUID = DB.getString("tag", "") + " " + tag1;
        // DB.putString("tag", tagUID);
        DB.putString(tag1.c_str(), "true");
        DB.end();

        Serial.println("Sucess registering " + tag1);
        digitalWrite(G_LED, LOW);

        blink(3, 2);
        delay(5000);
        modePrintPIN = true;
      } else {
        Serial.println("[Warming] tag not match");
        blink(3, 1);
        modePrintPIN = true;
      }
      RFIDMode = false;
      modePrintPIN = true;
    }  //END OF ADD RFID TAG

    // REMOVE RFID TAG
    if (keypressed == 'C') {
      allLedOff();
      digitalWrite(R1_LED, HIGH);
      Serial.println("Scan your RFID to remove! ");
      String tag1 = SCAN_TAG();
      if (tag1 == "") {
        RFIDMode = false;
        modePrintPIN = true;
        return;
      }
      Serial.println(tag1);
      delay(1000);
      digitalWrite(R2_LED, HIGH);
      Serial.println("[Info] Scan again to veritfy");
      String tag2 = SCAN_TAG();
      Serial.println(tag2);

      if (tag1 == tag2) {
        allLedOff();
        digitalWrite(G_LED, HIGH);
        Serial.println("[Info] tag match");
        Serial.println("Removing " + tag1 + " tag ID");

        DB.end();
        DB.begin("Credentials");
        DB.remove(tag2.c_str());
        // tagUID = DB.getString("tag", "") + " " + tag1;
        // DB.putString(tag2, "tagUID");
        DB.end();


        Serial.println("Sucess removing " + tag1);
        digitalWrite(G_LED, LOW);

        blink(3, 2);
        delay(5000);
        modePrintPIN = true;
      } else {
        Serial.println("[Warming] tag not match");
        blink(3, 1);
        modePrintPIN = true;
      }
      RFIDMode = false;
      modePrintPIN = true;
    }  //END OF REMOVE RFID TAG

    //Print WIFI MODE [AP 2, WIFI 1] And Start server [Blynk, WifiServer]
    if (keypressed == 'D') {

      Serial.println("Toggling WIFI");
      toggleWifi();

      if (wifiMode == 1) {  // AP mode
        Serial.println("CONNECTION TYPE: AP MODE");
        blink(2, 1);

      } else if (wifiMode == 2) {  //WIFI mode
        Serial.println("CONNECTION TYPE: WIFI MODE");
        delay(2000);
        Serial.println("Choice server 1 = Wifi Server, 2 = Blynk Server");
        digitalWrite(R1_LED, HIGH);
        String inputServer = GET_PINCODE();
        // "1":WifiServer, "2":BlynkServer
        if (inputServer == "1") {
          // (isBlynkOnRun) ? Blynk.end() : NULL;
          Serial.println("SERVER: Wifi Server");
          toggleServer("1");
          digitalWrite(G_LED, HIGH);
          blink(5, 2);  //blink for 5 times on g-led
        } else if (inputServer == "2") {
          (isWifiServerOnRun) ? WiFi.disconnect(true) : NULL;
          Serial.println("SERVER: Blynk Server");
          toggleServer("2");
          digitalWrite(G_LED, HIGH);
          blink(10, 2);
        } else {
          Serial.println("Invalid Choice");
        }

        allLedOff();
        printConfig(); 
        // blink(4, 1);

      } else if (wifiMode == 0) {  //Problem with stating wifi
        Serial.println("Cant start wifi...");
        blink(4, 1);
      }

      RFIDMode = false;
      modePrintPIN = true;
    }

  }  //END OF PIN MODE


  // RFID MODE
  if (RFIDMode == true) {

    if (modePrintRFID == true) {
      Serial.println("RFID mode");
      modePrintRFID = false;
    }

    // Look for new tag
    digitalWrite(R2_LED, HIGH);
    String tag = SCAN_TAG();

    if (tag == "") {
      RFIDMode = false;
      modePrintPIN = true;
      return;
    }

    Serial.println(tag);
    DB.begin("Credentials", false);
    String x = DB.getString(tag.c_str(), "false");
    Serial.println("currenttag UID:" + tagUID);
    Serial.println("is Exist:" + x);
    DB.end();
    if (x == "true") {
      // if (tagUID == tag) {

      allLedOff();

      digitalWrite(G_LED, HIGH);

      Serial.println("Tag Matched");
      Serial.println(tag);
      delay(1000);

      // tagsToSend
      tagsToSend = tagsToSend + " " + tag;

      OpenDoor();

      digitalWrite(G_LED, LOW);


    } else if (tag == "") {

    } else {
      // If UID of tag is not matched.
      digitalWrite(R1_LED, LOW);
      Serial.println("Wrong Tag Shown");
      delay(1000);
      digitalWrite(R2_LED, LOW);
    }

    RFIDMode = false;
    modePrintRFID = true;

  }  //END OF RFID MODE

}  // END OF LOOP()

void toggleServer(String a) {
  if (a == "1") {
    togggleWifiServer();
    isWifiServerOnRun = true;
    isBlynkOnRun = false;
  } else if (a == "2") {
    checkBlynkStatus();
    isWifiServerOnRun = false;
    isBlynkOnRun = true;
  }
}

void toggleWifi() {

  DB.begin("Credentials", false);
  ssid = DB.getString("SSID", "");
  password = DB.getString("Password", "");
  // ssid = "PLDTHOMEFIBRzyHRf";
  // password = "pldt@2g.WIFI";
  Serial.println(ssid);
  Serial.println(password);
  if (ssid == "" || password == "") {
    // if no wifi and pass open hotspot
    Serial.println("Setup AP");
    wifiAP();
    DB.putInt("wifi_mode", 1);  //AP mode
    DB.putInt("counter", 0);
    wifiMode = 1;
  } else {
    // Connect to Wi-Fi
    Serial.println("Setup WIFI");
    wifiConn(ssid, password);
    DB.putInt("wifi_mode", 2);  // WIFI mode
    DB.putInt("counter", 0);
    wifiMode = 2;
  }

  DB.end();
}

void togggleWifiServer() {
  server.begin();
  Serial.println("Server started");

  // is client connected
  server.on("/isOn", []() {
    Serial.println("CLIENT CONNECTED");
    server.send(200, "text/plain", "true");
  });

  // OpenDoor
  server.on("/d1", []() {
    //  bool mode = server.arg(0);
    // if (mode == "on") {
    if (!doorStatus) {

      server.send(200, "text/plain", "1");
      OpenDoor();
      server.send(200, "text/plain", "0");
    }
    // else if (mode == "off") {

    // }
  });
  
  server.on("/status", status); // status 
  server.on("/gp", gp); // generate vpin
  server.on("/sp", sp); // send vpin
  server.on("/wu", wu); // wifi update
  server.on("/isParty", isParty); // party mode
  
  server.onNotFound([]() {
    server.send(404, "text/plain", "Not found");
  });

  isServerOn = true;
  isWifiServerOnRun = true;
}
// SERVER FUNCTIONs
// "/status"  [doorStatus(0,1), isPartmpde(0,1), wifiMode(0,1), ssid(0, 1), tagsToSend(String)]
void status() {
  server.send(200, "html/plain",
              String(doorStatus) + "|" + String(isPartyMode) + "|" + String(wifiMode) + "|" + String(ssid) + "|" + String(tagsToSend));  //response true or false
  tagsToSend = " ";
  doorStatus = false;
}

void gp() {
  String a = GENERATE_VISITORS_PIN();
  Serial.println("New vPin: " + a);
  server.send(200, "text/plain", "");
}

void sp() {
  DB.begin("Credentials", false);
  // send current visitors pin
  String sendVPin = DB.getString("vPin", "");
  Serial.println("Sending vPin: " + sendVPin);
  server.send(200, "text/plain", sendVPin);
  DB.end();
}

void isParty() {
  // bool mode = server.arg(0);
  allLedOff();
  if (isPartyMode == false) {
    isPartyMode = true;
    doorStatus = true;
    digitalWrite(G_LED, HIGH);
    digitalWrite(Lock_LED, HIGH);
    Serial.println("PARTY MODE IS ON");
    server.send(200, "text/plain", "on");
  } else if (isPartyMode == true) {
    isPartyMode = false;
    doorStatus = false;
    digitalWrite(G_LED, LOW);
    digitalWrite(Lock_LED, LOW);
    Serial.println("PARTY MODE IS OFF");
    server.send(200, "text/plain", "off");
  }
}

void wu() {
  String ussid = server.arg(0);
  String upass = server.arg(1);
  String a = "SSID: " + ussid + "\nPASSWORD: " + upass + "\n";
  if (!ussid && !upass) {
    DB.begin("Credentials", false);

    server.send(200, "text/plain", a);
    Serial.println(a);

    DB.putString("SSID", ussid);
    DB.putString("Password", upass);

    DB.end();

    wifiConn(DB.getString("SSID", ""), DB.getString("Password", ""));

    server.send(200, "text/plain", "Connected");
  }
}



String SCAN_TAG() {
  String tag = "";
  bool a = true;
  startTimer = true;
  while (a) {
    if (startTimer) {
      if (mfrc522.PICC_IsNewCardPresent()) {             // (true, if RFID tag/card is present ) PICC = Proximity Integrated Circuit Card
        if (mfrc522.PICC_ReadCardSerial()) {             // true, if RFID tag/card was read
          for (byte i = 0; i < mfrc522.uid.size; ++i) {  // read id (in parts)
            tag.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
            tag.concat(String(mfrc522.uid.uidByte[i], HEX));
            (i < mfrc522.uid.size) ? a = false : true;
            startTimer = false;
          }
        }
      }
    } else {
      a = false;
    }
  }

  tag.replace(" ", "");
  return tag;
}

String GET_PINCODE() {
  String inputPinCode = "";

  Serial.println(" press # to enter");
  int count = 0;
  while (keypressed != '#' && count != 8) {
    keypressed = myKeypad.getKey();
    if (keypressed != NO_KEY && keypressed != '#') {
      digitalWrite(R1_LED, LOW);
      delay(200);
      digitalWrite(R1_LED, HIGH);
      Serial.print(keypressed);
      inputPinCode += keypressed;
    }
  }
  Serial.println();
  keypressed = NO_KEY;
  return inputPinCode;
}

void CHANGE_PINCODE() {
  Serial.println("Changing pinCode");
  delay(1000);
  Serial.println("Enter old pinCode");
  String pin = GET_PINCODE();  //verify the old pinCode first so you can change it

  if (pin == pinCode) {  // verifying the a value
    Serial.println("Changing pinCode");

    digitalWrite(R1_LED, HIGH);
    digitalWrite(R2_LED, HIGH);
    delay(2000);
    digitalWrite(R1_LED, LOW);
    digitalWrite(R2_LED, LOW);

    digitalWrite(R1_LED, HIGH);
    Serial.println("Insert new pin code!");
    String verify = GET_PINCODE();

    digitalWrite(R2_LED, HIGH);
    Serial.println("Insert again to confirm new pin code!");
    String verify2 = GET_PINCODE();

    bool changeCode = (verify == verify2) ? true : false;

    if (changeCode) {
      digitalWrite(G_LED, HIGH);

      // CHANGE CODE HERE
      DB.begin("Credentials", false);
      DB.putString("pincode", verify2);
      pinCode = DB.getString("pincode", "");
      Serial.println(pinCode);
      DB.end();

      Serial.println("pinCode Changed");
      printthis("WARNING: pinCode CHANGE!");
      delay(3000);
      allLedOff();
    } else {  //In case the new pinCodes aren't matching
      digitalWrite(R1_LED, LOW);
      Serial.println("pinCodes are not match!!");
      delay(2000);
      digitalWrite(R2_LED, LOW);
      digitalWrite(G_LED, LOW);
    }
  } else {  //In case the old pinCode is wrong you can't change it
    digitalWrite(R2_LED, LOW);
    Serial.println("Wrong old pincode");
    delay(2000);
    digitalWrite(R1_LED, LOW);
    digitalWrite(G_LED, LOW);
  }

  modePrintPIN = true;
}

// blink(timestoblink, {1:R1,2:G,3:R2}) per 3milis
void blink(int a, int m) {
  int x;
  switch (m) {
    case 1:
      x = R1_LED;
      break;
    case 2:
      x = G_LED;
      break;
    default:
      x = R2_LED;
      break;
  }
  for (int i = 0; i < a; i++) {
    digitalWrite(x, HIGH);
    delay(300);
    digitalWrite(x, LOW);
    delay(300);
  }
}

void allLedOff() {
  digitalWrite(R1_LED, LOW);
  digitalWrite(R2_LED, LOW);
  digitalWrite(G_LED, LOW);
}


// void OpenDoor() {  //Lock opening function open for 3s

// digitalWrite(G_LED, HIGH);
// digitalWrite(Lock_LED, HIGH);

// printthis("DOOR OPEN!");
// delay(3000);
// printthis("DOOR CLOSE!");

// digitalWrite(G_LED, LOW);
// digitalWrite(Lock_LED, LOW);

//   doorStatus = true;
//   modePrintPIN = true;
// }

String GENERATE_VISITORS_PIN() {
  String vPin;
  DB.begin("Credentials", false);
  DB.remove("vPin");
  for (byte i = 0; i < 4; i++) {
    vPin += String(random(10000000, 99999999)) + ((i == 3) ? "" : " ");
  }
  Serial.println("Visitors Pin: " + vPin);
  DB.putString("vPin", vPin);
  String updateVPin = DB.getString("vPin", "");
  DB.end();
  // Blynk.virtualWrite(V1, updateVPin);
  return updateVPin;
}

bool IS_IN_DB(String str, String key) {
  DB.begin("Credentials", false);
  String b = (key == "pin") ? DB.getString("vPin", "") : DB.getString("tag", "");
  String r = str;
  int x = b.indexOf(r);
  Serial.println("b: " + b);
  Serial.println("r: " + r);
  Serial.println("x: " + String(x));
  if ((x >= 0 && x <= 225) && (r.length() >= 6 && r.length() <= 8)) {
    b.remove(x, 8);
    if (key == "pin") {
      DB.remove("vPin");
      DB.putString("vPin", b);
      Serial.println("FUND AND REMOVE");
      // Blynk.virtualWrite(V1, b);
    }

    Serial.println("b: " + b);
    Serial.println("r: " + r);
    return true;
  } else {
    Serial.println("b: " + b);
    Serial.println("NOT FUND");
    return false;
  }
  DB.end();
}






WidgetTerminal terminal(V1);

void sendMailNotif(String pin_msg, String log_msg) {
  // checkBlynkStatus();
  //
  // if (pin_msg != " ") {
  //   Blynk.virtualWrite(VPIN_BUTTON_0, pin_msg); //send email and value to Vpin1
  // }

  // if (log_msg != " ") {
  //   Blynk.logEvent("door_status", log_msg); // send log and email
  // }
}

void printthis(String str) {
  Serial.println(str);
  //terminal.println(str);
  //terminal.flush();
}

// check blynk connection
void checkBlynkStatus() {
  bool isconnected = false;
  isconnected = Blynk.connected();
  if (isconnected == true) {
    Serial.println("Blynk Connected");
    isBlynkOnRun = true;
    isServerOn = true;
  } else {
    Serial.println("Blynk Not Connected");
    isBlynkOnRun = false;
    isServerOn = false;
  }
}

// Open/Close door btn
BLYNK_WRITE(V3) {
  int pinValue = param.asInt();  // assigning incoming value from pin V1 to a variable
  if (pinValue == 1) {
    sendMailNotif("Door Open", " ");
    OpenDoor();
    sendMailNotif("Door Close", " ");
    Blynk.virtualWrite(V3, 0);
  }
}

// // opt request btn
// BLYNK_WRITE(V2) {
//   int pinValue = param.asInt();  // assigning incoming value from pin V1 to a variable
//   if (pinValue == 1) {
//     Blynk.logEvent("otp", sendVisitorsPin());
//     delay(3000);
//     Blynk.virtualWrite(V2, 0);
//   }
// }

// BLYNK_WRITE(V1) {
//   // if (String("Marco") == param.asStr()) {
//   //   terminal.println("You said: 'Marco'");
//   //   terminal.println("I said: 'Polo'") ;
//   // }
//   terminal.println(sendVisitorsPin());
//   terminal.flush();
// }