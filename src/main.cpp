#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

#include "FS.h"
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

const char* ssid="TORRES PEREZ";
const char* password="0008151964";
char mqtt_server[]="m12.cloudmqtt.com";
const int led=16;
const int button=0 ;
const int sensor=12;
WiFiClient espClient;
WiFiClient telnet;
WiFiServer server(23);
PubSubClient client(espClient);
MDNSResponder mdns;

String realSize = String(ESP.getFlashChipRealSize());
String ideSize = String(ESP.getFlashChipSize());
bool flashCorrectlyConfigured = realSize.equals(ideSize);

bool shouldSaveConfig=false;


void setup_wifi(){
  Serial.println();
  Serial.print("conectandose a ");
  Serial.println(ssid);
  WiFi.begin(ssid,password);
  while(WiFi.status()!=WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("conexion a wifi establecida");
  Serial.println("direccion IP:");
  Serial.println(WiFi.localIP());
}

void handleTelnet(){
  if(server.hasClient()){
    if(!telnet || !telnet.connected()){
      if(telnet) telnet.stop();
      telnet=server.available();
    }
    else{
      server.available().stop();
    }
  }
}

void setup_mDNS(){
  if(mdns.begin("esp8266-01",WiFi.localIP())){
    Serial.println("mdns responder iniciado");
  }
}

void setup_arduinoOTA(){
  ArduinoOTA.onStart([]() {
   telnet.println("Start");
 });
 ArduinoOTA.onEnd([]() {
   telnet.println("\nEnd");
 });
 ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
   telnet.printf("Progress: %u%%\r", (progress / (total / 100)));
 });
 ArduinoOTA.onError([](ota_error_t error) {
   telnet.printf("Error[%u]: ", error);
   if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
   else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
   else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
   else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
   else if (error == OTA_END_ERROR) Serial.println("End Failed");
 });
 ArduinoOTA.begin();
 telnet.println("Ready");
 telnet.print("IP address: ");
 telnet.println(WiFi.localIP());
}

void saveConfigCallback(){
  shouldSaveConfig = true;
}

bool saveConfig(){
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["mqqt_server"]=mqtt_server;

  File configFile = SPIFFS.open("/config.json","w");
  if(!configFile){
    return false;
  }

  json.printTo(configFile);
  return true;
}

bool loadConfig(){
  File configFile = SPIFFS.open("/config.json","r");
  if(!configFile){
    return false;
  }

  size_t size=configFile.size();
  if(size>1024){
    return false;
  }
  std::unique_ptr<char[]> buf(new char[size]);

  configFile.readBytes(buf.get(), size);

  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& json = jsonBuffer.parseObject(buf.get());

  if(!json.success()){
    return false;
  }

  strcpy(mqtt_server,json["mqtt_server"]);
}

void forceConfigMode(){
  WiFi.disconnect();
  //ESP.restart();
  Serial.println("restart");
}

void callback(char* topic, byte* payload, unsigned int length){
  telnet.println();
  telnet.print("mensaje recibido [");
  telnet.print(topic);
  telnet.println("]");

  char json[length];
  StaticJsonBuffer<40> jsonBuffer;

  for(int i=0;i<length;i++){
    telnet.print((char)payload[i]);
    json[i]=(char)payload[i];
  }
  telnet.println();

  JsonObject& root = jsonBuffer.parseObject(json);
  bool status=root["status"];
  telnet.println(status);

  StaticJsonBuffer<100> jsonBufferRoot;
  JsonObject &root2 = jsonBufferRoot.createObject();

  if(status){

    root2["status"] = "true";
    String JSON;
    root2.printTo(JSON);
    const char *payload = JSON.c_str();

    digitalWrite(led,LOW);
    telnet.println("prender");
    Serial.println("prender");
    client.publish("foco_estado", payload);
  }
  else{

    root["status"] = "false";
    String JSON;
    root.printTo(JSON);
    const char *payload = JSON.c_str();

    digitalWrite(led, HIGH);
    telnet.println("apagar");
    Serial.println("apagar");
    client.publish("foco_estado", payload);
  }

}

void reconnect(){
  while(!client.connected()){
    telnet.println("intentando conexion a mqtt...");
    String clientId="focoESP8266";
    if(client.connect(clientId.c_str(),"xbvmyoxh","nJzjyl5r-7GD")){
      telnet.println("conectado");
      Serial.println("conectado");
      client.publish("foco_estado_conexion","esp8266 conectado");
      client.subscribe("encenderFoco");
    }
    else{
      telnet.print("conexion fallida, rc= ");
      telnet.print(client.state());
      telnet.println("inteta de nuevo en 5 segundos");
      delay(5000);
    }
  }
}

void setup(){
  Serial.begin(115200);
  Serial.println();
  Serial.println("proceso inicializado");
  if(!SPIFFS.begin()){
    Serial.println("no se pudo montar FS");
    return;
  }
  else{
    Serial.println("se montó la FS");
  }

  pinMode(led,OUTPUT);
  pinMode(sensor,INPUT);
  pinMode(button,INPUT);
  loadConfig();

  /*WiFiManager wifiManager;
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  WiFiManagerParameter customMqttServer("mqtt_server","mqtt server",mqtt_server,15);
  wifiManager.addParameter(&customMqttServer);

  wifiManager.autoConnect("esp-SEMARD","0008151964");
  strcpy(mqtt_server,customMqttServer.getValue());*/

  if(shouldSaveConfig){
    saveConfig();
  }

  setup_wifi();
  setup_mDNS();
  server.begin();
  server.setNoDelay(true);
  setup_arduinoOTA();
  client.setServer(mqtt_server, 12489);
  client.setCallback(callback);

}

void loop(){
  //Serial.println(flashCorrectlyConfigured);
  Serial.print("-");
  /*digitalWrite(led,LOW);
  delay(500);
  digitalWrite(led,HIGH);
  delay(500);*/
  if(digitalRead(button)==LOW){
    Serial.println("force config mode");
    //forceConfigMode();
  }
  handleTelnet();
  ArduinoOTA.handle();
  if(!client.connected()){
    reconnect();
  }
  client.loop();

  if(digitalRead(sensor)==LOW){
    telnet.println("HIGH");
    delay(500);
  }
  else{
    telnet.println("LOW");
    delay(500);
  }

}