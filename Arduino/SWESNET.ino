#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

//--------------- KONFIGURASI ----------------------------------------------------------------------
#define TRIAC_PIN 6
//kalo pake Adafruit MQTT Broker, topic biasanya diawali dengan prefix: <username>/feeds/<namatopic>
#define MQTT_TOPIC "your_topic"
#define MQTT_SERVER "io.adafruit.com"
#define MQTT_PORT 1883
#define MQTT_USERNAME "your_username"
#define MQTT_PASSWORD "your_password"
//--------------------------------------------------------------------------------------------------

ESP8266WebServer server(80);
struct configData{
  byte CONFIGURED;
  char WIFI_SSID[20];
  char WIFI_PASS[20];
};
volatile bool relayIsOn=false;
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;
void relayOn(){
  relayIsOn=true;
  digitalWrite(D6,HIGH);
  Serial.println(F("TRIAC is ON"));
}
void relayOff(){
  relayIsOn=false;
  digitalWrite(D6,LOW);
  Serial.println(F("TRIAC is OFF"));
}
void startAP(){
  String AP_NAME = "SWESNET-";
  AP_NAME += ESP.getChipId();
  WiFi.softAP(AP_NAME.c_str(), "swesnetwork");
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  server.on("/", [](){
    configData data;
    EEPROM.begin(512);
    EEPROM.get(0,data);
    String message = "<head><title>SWESNET</title><meta name='viewport' content='width=device-width, initial-scale=1'>";
    message += "<style>label{display:inline-block;width:120px;text-align:right;}input{margin-left:10px}</style></head>";
    if(server.args()>0){
      /*
      message += "<pre>";
      message += "URI: ";
      message += server.uri();
      message += "\nMethod: ";
      message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
      message += "\nArguments: ";
      message += server.args();
      message += "\n";
      */
      char ssid[20];
      char pass[20];
      bool configChanged=false;
      for ( uint8_t i = 0; i < server.args(); i++ ) {
        //message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
        if(server.argName(i)=="ssid"){
          sprintf(ssid,"%s",server.arg(i).c_str());
          if(strcmp(data.WIFI_SSID,ssid)!=0){
            configChanged=true;
            Serial.print(F("Saving SSID: "));
            Serial.println(ssid);
            strcpy(data.WIFI_SSID,ssid);
          }
        }else if(server.argName(i)=="pass"){
          sprintf(pass,"%s",server.arg(i).c_str());
          if(strcmp(data.WIFI_PASS,pass)!=0){
            configChanged=true;
            Serial.print(F("Saving PASS: "));
            Serial.println(pass);
            strcpy(data.WIFI_PASS,pass);
          }
        }else if(server.argName(i)=="relay"){
          if(server.arg(i)=="1"){
            relayOn();
          }else{
            relayOff();
          }
        }
      }
      //message+="</pre>";
      if(configChanged){
        data.CONFIGURED=1;
        EEPROM.put(0,data);
        EEPROM.commit();
        mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
        wifiReconnectTimer.once(2, connectToWifi);
      }
    }  
    message += "<h2>SWESNET CONFIG</h2>";
    message += "<form method='post' action='/'>";
    message += "<div><label>State : </label><input name='state' type='text' disabled='disabled' value='";
    if(WiFi.isConnected()){
      message += "CONNECTED";
    }else{
      message += "DISCONNECTED";
    }
    message += "'/></div>";
    message += "<div><label>Serial No : </label><input name='sn' type='text' disabled='disabled' value='";
    message += ESP.getChipId();
    message += "'/></div>";
    message += "<div><label>Router SSID : </label><input name='ssid' max-length=15 type='text' value='";
    message += data.WIFI_SSID;
    message += "'/></div>";
    message += "<div><label>Password : </label><input name='pass' max-length=15 type='password' value='";
    message += data.WIFI_PASS;
    message += "'/></div>";
    message += "<div><label>&nbsp;</label><input type='submit' value='Save'/></div>";
    message += "</form>";
    if(relayIsOn){
      message += "<button onclick=\"window.location='/?relay=0'\">Turn Off</button>";
    }else{
      message += "<button onclick=\"window.location='/?relay=1'\">Turn On</button>";
    }
    server.send ( 200, "text/html", message );
  });
  server.begin();
  Serial.println("HTTP server started");
}
void connectToWifi() {
  configData data;
  EEPROM.begin(512);
  EEPROM.get(0,data);
  if(data.CONFIGURED==1){
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(data.WIFI_SSID, data.WIFI_PASS);
  }
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");  
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);  
  String sub_topic = MQTT_TOPIC;
  sub_topic += ESP.getChipId();
  String pub_topic = topic;
  pub_topic += ESP.getChipId();
  
  uint16_t packetIdSub = mqttClient.subscribe(sub_topic.c_str(), 2);
  Serial.print("Subscribing at QoS 2, packetId: ");
  Serial.println(packetIdSub);
  mqttClient.publish(pub_topic.c_str(), 1, true, "1");
  Serial.println("Publishing at QoS 0");
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  if(payload[0]=='1'){
    relayOn();
  }else{
    relayOff();
  }
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void setup() {
  pinMode(D6,OUTPUT);
  EEPROM.begin(512);
  Serial.begin(115200);
  relayOn();
  startAP();
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onSubscribe(onMqttSubscribe);
  mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCredentials(MQTT_USERNAME, MQTT_PASSWORD);

  connectToWifi();
}

void loop() {
  server.handleClient();
}
