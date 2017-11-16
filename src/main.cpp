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

const char* ssid="TORRES PEREZ";//Progsistemas
const char* password="0008151964";//scndsemsept17
char mqtt_server[]="m12.cloudmqtt.com";
const int foco=16;
int cronometro=0;
String estado_foco;
WiFiClient espClient;
WiFiServer server(23);
PubSubClient client(espClient);
MDNSResponder mdns;
bool shouldSaveConfig=false;



void setup_wifi(){
  Serial.println();
  Serial.print("conectandose a ");
  Serial.println(ssid);

  WiFiManager wifiManager;
  //wifiManager.resetSettings();

  if (!wifiManager.autoConnect("Foco - Red alternativa")) {
    Serial.println("failed to connect and hit timeout");
    // reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  Serial.println();
  Serial.println("conexion a wifi establecida");
  Serial.println("direccion IP:");
  Serial.println(WiFi.localIP());
}


void setup_mDNS(){
  if(mdns.begin("esp8266-01",WiFi.localIP())){
    Serial.println("mdns responder iniciado");
  }
}

void setup_arduinoOTA(){
  ArduinoOTA.onStart([]() {
  Serial.println("OTA: Start");
 });
 ArduinoOTA.onEnd([]() {
   Serial.println("\nEnd");
 });
 ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
   Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
 });
 ArduinoOTA.onError([](ota_error_t error) {
   Serial.printf("Error[%u]: ", error);
   if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
   else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
   else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
   else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
   else if (error == OTA_END_ERROR) Serial.println("End Failed");
 });
 ArduinoOTA.begin();
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
  Serial.println();
  Serial.print("MQTT: mensaje recibido [");
  Serial.print(topic);
  Serial.println("]");

  char json[length];
  StaticJsonBuffer<40> jsonBuffer;

  for(int i=0;i<length;i++){
    Serial.print((char)payload[i]);
    json[i]=(char)payload[i];
  }
  Serial.println();

  JsonObject& root = jsonBuffer.parseObject(json);
  bool status=root["status"];
  Serial.println(status);

  StaticJsonBuffer<100> jsonBufferRoot;
  JsonObject &root2 = jsonBufferRoot.createObject();

  if(status){
    root2["status"] = true;
    String JSON;
    root2.printTo(JSON);
    const char *payload = JSON.c_str();

    estado_foco="Encendido";
    digitalWrite(foco,LOW);
    Serial.println("prender");
    client.publish("foco_cambio", payload);
  }
  else{
    root2["status"] = false;
    String JSON;
    root2.printTo(JSON);
    const char *payload = JSON.c_str();

    estado_foco="Apagado";

    digitalWrite(foco, HIGH);
    Serial.println("apagar");
    client.publish("foco_cambio", payload);
  }

}

void reconnect(){
  while(!client.connected()){
    Serial.println("intentando conexion a mqtt...");
    String clientId="focoESP8266";
    if(client.connect(clientId.c_str(),"xbvmyoxh","nJzjyl5r-7GD")){
      //telnet.println("conectado");
      Serial.println("conectado");
      client.publish("foco_estado_conexion","esp8266 conectado");
      client.subscribe("encenderFoco");
    }
    else{
      Serial.print("conexion fallida, rc= ");
      Serial.print(client.state());
      Serial.println("inteta de nuevo en 5 segundos");
      delay(5000);
    }
  }
}

void setup(){
  Serial.begin(115200);
  Serial.println();
  Serial.println("proceso inicializado");
  estado_foco="Apagado";

  if(!SPIFFS.begin()){
    Serial.println("no se pudo montar FS");
    return;
  }
  else{
    Serial.println("se montó la FS");
  }

  pinMode(foco,OUTPUT);
  digitalWrite(foco, HIGH);
  loadConfig();

  //WiFiManager wifiManager;
  /*wifiManager.setSaveConfigCallback(saveConfigCallback);

  WiFiManagerParameter customMqttServer("mqtt_server","mqtt server",mqtt_server,15);
  wifiManager.addParameter(&customMqttServer);

  wifiManager.autoConnect("esp-SEMARD","0008151964");
  strcpy(mqtt_server,customMqttServer.getValue());/**/

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
  Serial.print("-");
  delay(500);
  cronometro++;
  if(cronometro==10){
    cronometro=0;
    StaticJsonBuffer<100> jsonBufferRoot;
    JsonObject &root = jsonBufferRoot.createObject();
    if(estado_foco=="Encendido"){
      root["status"] = true;
    }
    else{
      root["status"] = false;
    }
    String JSON;
    root.printTo(JSON);
    const char *payload = JSON.c_str();
    client.publish("foco_estado", payload);
  }
  ArduinoOTA.handle();
  if(!client.connected()){
    reconnect();
  }
  client.loop();
}
