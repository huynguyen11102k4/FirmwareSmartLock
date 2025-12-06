#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <MFRC522.h>
#include <SPI.h>
#include <Servo.h>
#include <Keypad.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <SPIFFS.h>
#include <vector>
#include <algorithm>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include <time.h>
#include <ca_cert.h>

#define SS_PIN 5
#define RST_PIN 22
#define SERVO_PIN 21
#define LED_PIN 15
#define BATTERY_PIN 34

#define SERVICE_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
#define CHAR_CONFIG_UUID "0000ffe1-0000-1000-8000-00805f9b34fb"
#define CHAR_NOTIFY_UUID "0000ffe2-0000-1000-8000-00805f9b34fb"

using namespace std;

const char *NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 25200;
const int DAYLIGHT_OFFSET_SEC = 0;

struct AppConfig
{
  String wifi_ssid;
  String wifi_pass;
  String mqtt_host;
  int mqtt_port;
  String mqtt_user;
  String mqtt_pass;
  String topic_prefix;
};

struct PasscodeTemp
{
  String code;
  String type;
  String validity;
};

MFRC522 mfrc522(SS_PIN, RST_PIN);

const byte ROWS = 4, COLS = 4;
char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}};
byte rowPins[ROWS] = {27, 26, 25, 33};
byte colPins[COLS] = {32, 17, 16, 4};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

BLECharacteristic *pConfig;
BLECharacteristic *pNotify;

bool bleActive = false;

String mqttHost = "";
int mqttPort = 0;
String mqttUser = "";
String mqttPass = "";
String mqttTopic = "door/123";

WiFiClientSecure espClient;
PubSubClient client(espClient);

String deviceMAC = "";
String masterPasscode = "";
vector<String> iccards;
vector<PasscodeTemp> tempPasscodes;

AppConfig appConfig;

const char *TEMP_PASS_FILE = "/temp_passcodes.json";
const char *MASTER_FILE = "/master.json";
const char *CARD_FILE = "/iccards.json";
const char *MQTT_SERVER = "d2684409b6644e89b97ca34b695085ae.s1.eu.hivemq.cloud";
const char *CONFIG_FILE = "/config.json";

bool wifiConfigured = false;
bool mqttConfigured = false;

bool swipeAddMode = false;
unsigned long addSwipeTimeout = 0;
String firstSwipeUid = "";

bool saveConfig()
{
  DynamicJsonDocument doc(512);

  doc["wifi_ssid"] = appConfig.wifi_ssid;
  doc["wifi_pass"] = appConfig.wifi_pass;
  doc["mqtt_host"] = appConfig.mqtt_host;
  doc["mqtt_port"] = appConfig.mqtt_port;
  doc["mqtt_user"] = appConfig.mqtt_user;
  doc["mqtt_pass"] = appConfig.mqtt_pass;
  doc["topic_prefix"] = appConfig.topic_prefix;

  File f = SPIFFS.open(CONFIG_FILE, "w");
  if (!f)
    return false;

  serializeJson(doc, f);
  f.close();
  return true;
}

bool loadConfig()
{
  Serial.println("[Config] Loading from SPIFFS...");

  if (!SPIFFS.exists(CONFIG_FILE))
  {
    Serial.println("[Config] File not found!");
    return false;
  }

  File f = SPIFFS.open(CONFIG_FILE, "r");
  if (!f)
  {
    Serial.println("[Config] Failed to open file!");
    return false;
  }

  size_t fileSize = f.size();
  Serial.println("[Config] File size: " + String(fileSize) + " bytes");

  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, f);
  f.close();

  if (error)
  {
    Serial.print("[Config] JSON parse error: ");
    Serial.println(error.c_str());
    return false;
  }

  Serial.println("[Config] JSON content:");
  serializeJsonPretty(doc, Serial);
  Serial.println();

  appConfig.wifi_ssid = doc["wifi_ssid"].as<String>();
  appConfig.wifi_pass = doc["wifi_pass"].as<String>();
  appConfig.mqtt_host = doc["mqtt_host"].as<String>();
  appConfig.mqtt_port = doc["mqtt_port"] | 8883;
  appConfig.mqtt_user = doc["mqtt_user"].as<String>();
  appConfig.mqtt_pass = doc["mqtt_pass"].as<String>();
  appConfig.topic_prefix = doc["topic_prefix"].as<String>();

  Serial.println("[Config] Loaded values:");
  Serial.println("  wifi_ssid: " + appConfig.wifi_ssid);
  Serial.println("  wifi_pass: " + String(appConfig.wifi_pass.length()) + " chars");
  Serial.println("  mqtt_host: " + appConfig.mqtt_host);
  Serial.println("  mqtt_port: " + String(appConfig.mqtt_port));
  Serial.println("  mqtt_user: " + appConfig.mqtt_user);
  Serial.println("  mqtt_pass: " + String(appConfig.mqtt_pass.length()) + " chars");
  Serial.println("  topic_prefix: " + appConfig.topic_prefix);

  return true;
}

void setupSecureClient()
{
  espClient.setCACert(hivemq_root_ca);
  espClient.setTimeout(20);
  espClient.setHandshakeTimeout(30);
}

bool hasMasterPasscode()
{
  return masterPasscode.length() >= 4;
}

bool saveMasterPasscode()
{
  File f = SPIFFS.open(MASTER_FILE, "w");
  if (!f)
    return false;
  f.print(masterPasscode);
  f.close();
  return true;
}

bool loadMasterPasscode()
{
  if (!SPIFFS.exists(MASTER_FILE))
    return false;
  File f = SPIFFS.open(MASTER_FILE, "r");
  if (!f)
    return false;
  masterPasscode = f.readString();
  f.close();
  masterPasscode.trim();
  return masterPasscode.length() >= 4;
}

time_t parseDateTimeToEpoch(const String &dt)
{
  int d, m, y, h, min;
  if (sscanf(dt.c_str(), "%d/%d/%d %d:%d", &d, &m, &y, &h, &min) != 5)
    return 0;
  struct tm timeinfo = {0};
  timeinfo.tm_year = y - 1900;
  timeinfo.tm_mon = m - 1;
  timeinfo.tm_mday = d;
  timeinfo.tm_hour = h;
  timeinfo.tm_min = min;
  timeinfo.tm_sec = 0;
  timeinfo.tm_isdst = -1;
  return mktime(&timeinfo);
}

bool loadList(const char *file, vector<String> &list)
{
  if (!SPIFFS.exists(file))
    return false;
  File f = SPIFFS.open(file, "r");
  if (!f)
    return false;
  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, f))
  {
    f.close();
    return false;
  }
  f.close();
  list.clear();
  for (JsonVariant v : doc.as<JsonArray>())
    list.push_back(v.as<String>());
  return true;
}

bool saveList(const char *file, const vector<String> &list)
{
  DynamicJsonDocument doc(2048);
  JsonArray arr = doc.to<JsonArray>();
  for (const String &s : list)
    arr.add(s);
  File f = SPIFFS.open(file, "w");
  if (!f)
    return false;
  if (serializeJson(doc, f) == 0)
  {
    f.close();
    return false;
  }
  f.close();
  return true;
}

bool loadTempPasscodes()
{
  if (!SPIFFS.exists(TEMP_PASS_FILE))
    return false;
  File f = SPIFFS.open(TEMP_PASS_FILE, "r");
  if (!f)
    return false;
  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, f))
  {
    f.close();
    return false;
  }
  f.close();
  tempPasscodes.clear();
  for (JsonVariant v : doc.as<JsonArray>())
  {
    PasscodeTemp pc;
    pc.code = v["code"].as<String>();
    pc.type = v["type"].as<String>();
    pc.validity = v["validity"].as<String>();
    tempPasscodes.push_back(pc);
  }
  return true;
}

bool saveTempPasscodes()
{
  DynamicJsonDocument doc(2048);
  JsonArray arr = doc.to<JsonArray>();
  for (const PasscodeTemp &pc : tempPasscodes)
  {
    JsonObject obj = arr.createNestedObject();
    obj["code"] = pc.code;
    obj["type"] = pc.type;
    obj["validity"] = pc.validity;
  }
  File f = SPIFFS.open(TEMP_PASS_FILE, "w");
  if (!f)
    return false;
  if (serializeJson(doc, f) == 0)
  {
    f.close();
    return false;
  }
  f.close();
  return true;
}

void publishState(const String &s, const String &reason = "")
{
  if (!client.connected())
    return;
  DynamicJsonDocument doc(256);
  doc["state"] = s;
  doc["reason"] = reason;
  String out;
  serializeJson(doc, out);
  client.publish((mqttTopic + "/state").c_str(), out.c_str());
}

void publishLog(const String &ev, const String &method, const String &det = "")
{
  if (!client.connected())
    return;
  DynamicJsonDocument doc(256);
  doc["event"] = ev;
  doc["method"] = method;
  doc["detail"] = det;
  doc["millis"] = (long)millis();
  String out;
  serializeJson(doc, out);
  client.publish((mqttTopic + "/log").c_str(), out.c_str());
}

String getUID()
{
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++)
  {
    if (mfrc522.uid.uidByte[i] < 0x10)
      uid += "0";
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  return uid;
}

bool checkIC(const String &uid)
{
  return find(iccards.begin(), iccards.end(), uid) != iccards.end();
}

int readBatteryPercent()
{
  int raw = analogRead(BATTERY_PIN);
  float v = (raw / 4095.0) * 3.3 * 2;
  float percent = (v - 3.3) / (4.2 - 3.3) * 100;
  if (percent < 0)
    percent = 0;
  if (percent > 100)
    percent = 100;
  return (int)percent;
}

void publishBattery()
{
  if (!client.connected())
    return;
  DynamicJsonDocument doc(64);
  doc["battery"] = readBatteryPercent();
  String out;
  serializeJson(doc, out);
  client.publish((mqttTopic + "/battery").c_str(), out.c_str());
}

String pinBuffer = "";
int failedCount = 0;
unsigned long lockoutUntil = 0;

bool checkPIN(String pin)
{
  if (pin == masterPasscode)
    return true;
  for (int i = 0; i < tempPasscodes.size(); i++)
  {
    PasscodeTemp &p = tempPasscodes[i];
    if (p.code == pin)
    {
      if (p.type == "one-time")
      {
        tempPasscodes.erase(tempPasscodes.begin() + i);
        saveTempPasscodes();
        return true;
      }
      if (p.type == "timed")
      {
        DynamicJsonDocument doc(256);
        deserializeJson(doc, p.validity);
        time_t start = doc["start"] | 0;
        time_t end = doc["end"] | 0;
        time_t now = time(nullptr);
        if (now >= start && now <= end)
          return true;
      }
      else
      {
        return true;
      }
    }
  }
  return false;
}

Servo lockServo;

void unlock(const String &method)
{
  digitalWrite(LED_PIN, HIGH);
  lockServo.write(90);
  publishState("unlocked", method);
  publishLog("unlock", method);
  delay(5000);
  lockServo.write(0);
  digitalWrite(LED_PIN, LOW);
  publishState("locked", "auto");
}

WebServer server(80);

void handleInfo()
{
  DynamicJsonDocument doc(256);
  doc["mac"] = deviceMAC;
  doc["topic"] = mqttTopic;
  doc["battery"] = readBatteryPercent();
  doc["wifi"] = (WiFi.status() == WL_CONNECTED);
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleNotFound()
{
  server.send(404, "text/plain", "Not found");
}

void publishPasscodeList()
{
  if (!client.connected())
    return;
  DynamicJsonDocument doc(4096);
  JsonArray array = doc.to<JsonArray>();
  if (hasMasterPasscode())
  {
    JsonObject obj = array.createNestedObject();
    obj["code"] = masterPasscode;
    obj["type"] = "permanent";
    obj["validity"] = "";
    obj["status"] = "Active";
  }
  time_t now = time(nullptr);
  bool changed = false;
  for (auto it = tempPasscodes.begin(); it != tempPasscodes.end();)
  {
    bool expired = (it->type == "timed" &&
                    parseDateTimeToEpoch(it->validity) <= now);
    if (expired || it->type == "one-time")
    {
      it = tempPasscodes.erase(it);
      changed = true;
      continue;
    }
    JsonObject obj = array.createNestedObject();
    obj["code"] = it->code;
    obj["type"] = it->type;
    obj["validity"] = it->validity;
    obj["status"] = "Active";
    ++it;
  }
  if (changed)
    saveTempPasscodes();
  String payload;
  serializeJson(array, payload);
  client.publish((mqttTopic + "/passcodes/list").c_str(), payload.c_str(), true);
}

void publishICCardList()
{
  if (!client.connected())
    return;
  DynamicJsonDocument doc(1024);
  JsonArray arr = doc.to<JsonArray>();
  for (const String &card : iccards)
  {
    JsonObject obj = arr.createNestedObject();
    obj["id"] = card;
    obj["name"] = "Card #" + card.substring(0, 8);
    obj["status"] = "Active";
  }
  String out;
  serializeJson(doc, out);
  client.publish((mqttTopic + "/iccards/list").c_str(), out.c_str(), true);
}

class ConfigCallback : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *c)
  {
    std::string value = c->getValue();
    String json = String(value.c_str());

    Serial.println("==========================================");
    Serial.println("Config received! Length: " + String(json.length()));
    Serial.println("Content: " + json);
    Serial.println("==========================================");

    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, json);

    if (error)
    {
      Serial.println("JSON parse error: " + String(error.c_str()));
      String errorMsg = "error: invalid json - " + String(error.c_str());

      delay(200);
      pNotify->setValue(errorMsg.c_str());
      pNotify->notify();
      delay(100);

      Serial.println("Sent error notification");
      return;
    }

    String ssid = doc["wifi_ssid"] | "";
    String pass = doc["wifi_pass"] | "";
    String broker = doc["mqtt_broker"] | "";
    mqttUser = doc["mqtt_user"] | "";
    mqttPass = doc["mqtt_pass"] | "";

    if (doc.containsKey("topic_prefix"))
    {
      mqttTopic = doc["topic_prefix"].as<String>();
    }

    int colon = broker.indexOf(':');
    if (colon != -1)
    {
      mqttHost = broker.substring(0, colon);
      mqttPort = broker.substring(colon + 1).toInt();
    }
    else
    {
      mqttHost = broker;
      mqttPort = 8883;
    }

    Serial.println("Parsed successfully!");
    Serial.println("SSID: " + ssid);
    Serial.println("Broker: " + mqttHost + ":" + String(mqttPort));
    Serial.println("Topic: " + mqttTopic);

    appConfig.wifi_ssid = ssid;
    appConfig.wifi_pass = pass;
    appConfig.mqtt_host = mqttHost;
    appConfig.mqtt_port = mqttPort;
    appConfig.mqtt_user = mqttUser;
    appConfig.mqtt_pass = mqttPass;
    appConfig.topic_prefix = mqttTopic;

    if (saveConfig())
    {
      Serial.println("[Config] Saved to SPIFFS!");
    }
    else
    {
      Serial.println("[Config] SAVE FAILED!");
    }

    delay(800);

    pNotify->setValue("OK");
    pNotify->notify();
    Serial.println("OK notification sent!");

    if (ssid.length() > 0)
    {
      Serial.println("Starting WiFi connection...");
      WiFi.disconnect(true);
      delay(100);
      WiFi.begin(ssid.c_str(), pass.c_str());
      wifiConfigured = true;
      Serial.println("[WiFi] Connecting...");
    }

    if (mqttHost.length() > 0)
    {
      client.setServer(mqttHost.c_str(), mqttPort);
      mqttConfigured = true;
      Serial.println("[MQTT] Updated server: " + mqttHost + ":" + String(mqttPort));
    }

    Serial.println("Config callback completed!");
  }
};

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("[MQTT] Message received: ");
  Serial.println(topic);

  Serial.print("[MQTT] Payload: ");
  for (unsigned int i = 0; i < length; i++)
    Serial.print((char)payload[i]);
  Serial.println();

  String payloadStr = "";
  for (unsigned int i = 0; i < length; i++)
    payloadStr += (char)payload[i];
  String topicStr = String(topic);
  String prefix = mqttTopic;

  if (topicStr == prefix + "/passcodes")
  {
    Serial.println("[MQTT] Processing passcode action...");

    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, payloadStr))
    {
      Serial.println("[MQTT] JSON parse failed!");
      return;
    }

    String action = doc["action"].as<String>();
    String type = doc["type"].as<String>();
    Serial.println("[MQTT] Action: " + action + ", Type: " + type);

    if (action == "add" && type == "permanent")
    {
      String newCode = doc["code"].as<String>();
      String oldCode = doc["old_code"] | "123456";

      Serial.println("[MQTT] New code: " + newCode);
      Serial.println("[MQTT] Has master: " + String(hasMasterPasscode()));

      if (!hasMasterPasscode())
      {
        if (newCode.length() < 4 || newCode.length() > 10)
          return;
        masterPasscode = newCode;
        Serial.println("Master passcode set to: " + masterPasscode);
        saveMasterPasscode();
        publishPasscodeList();
        publishLog("master_set", "first_time", newCode);
        return;
      }

      if (oldCode.isEmpty() || oldCode != masterPasscode)
      {
        client.publish((appConfig.topic_prefix + "/passcodes/error").c_str(),
                       "{\"error\":\"old_master_required\"}");
        return;
      }

      masterPasscode = newCode;
      Serial.println("Master passcode changed to: " + masterPasscode);
      saveMasterPasscode();
      publishPasscodeList();
      publishLog("master_changed", "success", "");
      return;
    }

    if (action == "add" && (type == "one-time" || type == "timed"))
    {
      PasscodeTemp pc;
      pc.code = doc["code"].as<String>();
      pc.type = type;
      if (type == "timed")
      {
        time_t exp = parseDateTimeToEpoch(doc["validity"].as<String>());
        pc.validity = String(exp);
      }
      else
        pc.validity = "";
      tempPasscodes.push_back(pc);
      saveTempPasscodes();
      publishPasscodeList();
      return;
    }

    if (action == "delete")
    {
      String code = doc["code"].as<String>();
      if (hasMasterPasscode() && code == masterPasscode)
      {
        masterPasscode = "";
        SPIFFS.remove(MASTER_FILE);
        publishPasscodeList();
        return;
      }
      auto it = find_if(tempPasscodes.begin(), tempPasscodes.end(),
                        [code](const PasscodeTemp &pc)
                        { return pc.code == code; });
      if (it != tempPasscodes.end())
        tempPasscodes.erase(it);
      saveTempPasscodes();
      publishPasscodeList();
    }
  }
  else if (topicStr.endsWith("/passcodes/request"))
    publishPasscodeList();

  else if (topicStr == prefix + "/iccards")
  {
    DynamicJsonDocument doc(512);
    if (deserializeJson(doc, payloadStr))
      return;
    String action = doc["action"] | "";
    String id = doc["id"] | "";

    Serial.println("[MQTT] Processing IC card action...");
    Serial.println("[MQTT] Action: " + action + ", ID: " + id);
    if (action == "add" && id.length() > 0)
    {
      String uid = id;
      uid.replace(":", "");
      uid.toUpperCase();
      if (find(iccards.begin(), iccards.end(), uid) == iccards.end())
      {
        iccards.push_back(uid);
        saveList(CARD_FILE, iccards);
        publishLog("card_added", "mqtt", uid);
      }
    }
    else if (action == "delete" && !id.isEmpty())
    {
      String uid = id;
      uid.replace(":", "");
      uid.toUpperCase();
      iccards.erase(remove(iccards.begin(), iccards.end(), uid), iccards.end());
      saveList(CARD_FILE, iccards);
      publishLog("card_deleted", "mqtt", uid);
    }
    else if (action == "start_swipe_add")
    {
      Serial.println("[MQTT] Starting swipe-to-add mode for IC cards...");
      swipeAddMode = true;
      addSwipeTimeout = millis() + 60000;
      firstSwipeUid = "";
      String notifyTopic = mqttTopic + "iccards/status";
      client.publish(notifyTopic.c_str(), "swipe_add_started");
    }
  }
  else if (topicStr == prefix + "/iccards/request")
    publishICCardList();
}

void reconnect()
{
  if (!wifiConfigured)
    return;
  if (WiFi.status() != WL_CONNECTED)
    return;
  // if (!mqttConfigured || mqttHost.length() == 0 || mqttPort == 0) return;

  // if (client.connected()) {
  //   client.loop();
  //   return;
  // }

  static unsigned long lastAttempt = 0;
  if (millis() - lastAttempt < 5000)
    return;
  lastAttempt = millis();

  Serial.println("\n========================================");
  Serial.println("[MQTT] Attempting connection to broker...");
  Serial.print("[MQTT] Host: ");
  Serial.print(appConfig.mqtt_host);
  Serial.print(" Port: ");
  Serial.println(appConfig.mqtt_port);
  Serial.print("[MQTT] User: ");
  Serial.println(appConfig.mqtt_user);
  Serial.print("[MQTT] Topic prefix: ");
  Serial.println(appConfig.topic_prefix);
  Serial.println("========================================");

  setupSecureClient();
  client.setServer(appConfig.mqtt_host.c_str(), appConfig.mqtt_port);

  String clientId = "ESP32DoorLock-" + deviceMAC;

  bool connected = client.connect(
      clientId.c_str(),
      appConfig.mqtt_user.c_str(),
      appConfig.mqtt_pass.c_str());

  if (connected)
  {
    Serial.println("\n[MQTT] ✓ Connected successfully!");
    Serial.println("[MQTT] Now subscribing to topics...\n");

    // Subscribe từng topic với kiểm tra
    String topic1 = appConfig.topic_prefix + "/passcodes";
    bool sub1 = client.subscribe(topic1.c_str(), 1);
    Serial.println("[MQTT] Subscribe [" + topic1 + "] -> " + (sub1 ? "OK" : "FAILED"));

    String topic2 = appConfig.topic_prefix + "/passcodes/request";
    bool sub2 = client.subscribe(topic2.c_str(), 1);
    Serial.println("[MQTT] Subscribe [" + topic2 + "] -> " + (sub2 ? "OK" : "FAILED"));

    String topic3 = appConfig.topic_prefix + "/iccards";
    bool sub3 = client.subscribe(topic3.c_str(), 1);
    Serial.println("[MQTT] Subscribe [" + topic3 + "] -> " + (sub3 ? "OK" : "FAILED"));

    String topic4 = appConfig.topic_prefix + "/iccards/request";
    bool sub4 = client.subscribe(topic4.c_str(), 1);
    Serial.println("[MQTT] Subscribe [" + topic4 + "] -> " + (sub4 ? "OK" : "FAILED"));

    Serial.println("\n[MQTT] Callback function address: " + String((unsigned long)mqttCallback, HEX));
    Serial.println("[MQTT] Waiting for messages...\n");

    DynamicJsonDocument info(256);
    info["mac"] = deviceMAC;
    info["topic"] = appConfig.topic_prefix;
    info["battery"] = readBatteryPercent();
    info["version"] = 1;
    String out;
    serializeJson(info, out);
    client.publish((appConfig.topic_prefix + "/info").c_str(), out.c_str(), true);

    publishState("locked", "startup");
    publishBattery();
    publishPasscodeList();
    publishICCardList();

    Serial.println("========================================\n");
  }
  else
  {
    Serial.print("[MQTT] ✗ Failed, rc=");
    Serial.println(client.state());
    Serial.println("========================================\n");
  }
}

unsigned long lastBattery = 0;
unsigned long lastSync = 0;

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("Smart Lock Starting...");

  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // SPIFFS.format();

  SPIFFS.begin(true);
  loadMasterPasscode();
  loadList(CARD_FILE, iccards);
  loadTempPasscodes();

  if (!hasMasterPasscode())
  {
    masterPasscode = "123456";
    saveMasterPasscode();
  }

  SPI.begin();
  mfrc522.PCD_Init();

  lockServo.attach(SERVO_PIN);
  lockServo.write(0);

  WiFi.mode(WIFI_STA);
  delay(100);

  deviceMAC = WiFi.macAddress();
  String mac2 = deviceMAC;
  mac2.replace(":", "");
  mac2.toLowerCase();
  mqttTopic = "door/" + mac2.substring(0, 6);

  bool hasConfig = loadConfig();

  if (!hasConfig || appConfig.wifi_ssid.length() == 0)
  {
    Serial.println("\n========================================");
    Serial.println("SETUP MODE - Waiting for BLE config");
    Serial.println("========================================\n");

    bleActive = true;

    BLEDevice::init("SMARTLOCK");
    BLEServer *serverBle = BLEDevice::createServer();
    BLEService *svc = serverBle->createService(SERVICE_UUID);

    pConfig = svc->createCharacteristic(
        CHAR_CONFIG_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_READ);
    pConfig->addDescriptor(new BLE2902());

    pNotify = svc->createCharacteristic(
        CHAR_NOTIFY_UUID,
        BLECharacteristic::PROPERTY_NOTIFY |
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_WRITE);

    BLE2902 *cccd = new BLE2902();
    cccd->setNotifications(true);
    cccd->setIndications(false);
    pNotify->addDescriptor(cccd);

    pConfig->setCallbacks(new ConfigCallback());
    svc->start();

    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(SERVICE_UUID);
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);
    adv->setMinPreferred(0x12);
    adv->start();

    Serial.println("BLE advertising started");
  }
  else
  {
    Serial.println("\n========================================");
    Serial.println("NORMAL MODE - Connecting to WiFi/MQTT");
    Serial.println("========================================\n");

    bleActive = false;

    BLEDevice::deinit(true);
    delay(500);

    if (appConfig.wifi_ssid.length() > 0)
    {
      WiFi.begin(appConfig.wifi_ssid.c_str(), appConfig.wifi_pass.c_str());
      wifiConfigured = true;

      Serial.print("Connecting to WiFi");
      unsigned long startTime = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startTime < 20000)
      {
        Serial.print(".");
        delay(500);
      }

      if (WiFi.status() == WL_CONNECTED)
      {
        Serial.println("\nWiFi connected!");
        Serial.println("IP: " + WiFi.localIP().toString());
      }
      else
      {
        Serial.println("\nWiFi connection failed!");
      }
    }

    // if (appConfig.mqtt_host.length() > 0) {
    //   espClient.setCACert(hivemq_root_ca);
    //   espClient.setTimeout(30);
    //   espClient.setHandshakeTimeout(40);

    //   // client.setServer(appConfig.mqtt_host.c_str(), appConfig.mqtt_port);
    // client.setCallback(mqttCallback);
    // client.setKeepAlive(60);
    // client.setSocketTimeout(20);

    //   mqttConfigured = true;
    //   mqttTopic = appConfig.topic_prefix;
    //   Serial.println("MQTT configured");
    // }
    reconnect();
    client.setCallback(mqttCallback);
    client.setKeepAlive(60);
    client.setSocketTimeout(20);
  }

  server.on("/info", HTTP_GET, handleInfo);
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop()
{
  // server.handleClient();

  if (WiFi.status() == WL_CONNECTED)
  {
    if (!client.connected())
    {
      BLEDevice::deinit(true);
      delay(500);
      reconnect();
    }
    client.loop();
  }

  if (millis() - lastBattery > 60000)
  {
    publishBattery();
    lastBattery = millis();
  }

  if (millis() - lastSync > 300000)
  {
    publishPasscodeList();
    lastSync = millis();
  }

  unsigned long now = millis();

  if (now > lockoutUntil)
  {
    char k = keypad.getKey();
    if (k)
    {
      Serial.println(k);
      if (k == '*')
        pinBuffer = "";
      else if (k == '#')
      {
        if (checkPIN(pinBuffer))
        {
          failedCount = 0;
          unlock("pin");
          Serial.println("PIN correct");
        }
        else
        {
          Serial.println("PIN incorrect");
          failedCount++;
          if (failedCount >= 5)
          {
            lockoutUntil = now + 30000;
            failedCount = 0;
          }
        }
        pinBuffer = "";
      }
      else if (k >= '0' && k <= '9')
      {
        if (pinBuffer.length() < 6)
          pinBuffer += k;
      }
    }
  }

  if (swipeAddMode)
  {
    if (now > addSwipeTimeout)
    {
      swipeAddMode = false;
      firstSwipeUid = "";
      String notifyTopic = mqttTopic + "/iccards/status";
      client.publish(notifyTopic.c_str(), "swipe_add_timeout");
      Serial.println("[MQTT] Swipe-to-add mode timed out.");
    }
  }
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial())
  {
    String uid = getUID();
    uid.replace(":", "");
    uid.toUpperCase();
    Serial.println("Card UID: " + uid);
    if (swipeAddMode)
    {
      if (firstSwipeUid.length() == 0)
      {
        firstSwipeUid = uid;
        addSwipeTimeout = now + 60000;
        Serial.println("First swipe: " + uid);
        String notifyTopic = mqttTopic + "/iccards/status";
        client.publish(notifyTopic.c_str(), "swipe_first_detected");
      }
      else if (firstSwipeUid == uid)
      {
        if (find(iccards.begin(), iccards.end(), uid) == iccards.end())
        {
          iccards.push_back(uid);
          saveList(CARD_FILE, iccards);
          publishICCardList();
        }
        swipeAddMode = false;
        firstSwipeUid = "";
        String notifyTopic = mqttTopic + "/iccards/status";
        client.publish(notifyTopic.c_str(), "swipe_add_success");
        publishLog("card_added", "swipe", uid);
        Serial.println("IC card added via swipe: " + uid);
      }
      else
      {
        swipeAddMode = false;
        firstSwipeUid = "";
        String notifyTopic = mqttTopic + "/iccards/status";
        client.publish(notifyTopic.c_str(), "swipe_add_failed");
        Serial.println("Swipe-to-add failed: UIDs do not match.");
      }
    }

    if (checkIC(uid))
      unlock("card");
    else
    {
      if (iccards.empty())
      {
        iccards.push_back(uid);
        saveList(CARD_FILE, iccards);
      }
    }
    publishLog("card_scan", "card", uid);
    mfrc522.PICC_HaltA();
  }

  yield();
}
