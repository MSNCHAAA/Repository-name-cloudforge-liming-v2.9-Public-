// >>> CloudForge Home v4.1 — see main message for full comments <<<
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <U8g2lib.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define LED_NET 2
#define LED_WARN 25
#define LED_ALERT 26
#define LVL_LOW 32
#define LVL_MID 33
#define LVL_HIGH 34
#define IN_LOW_ALARM 23
#define IN_HIGH_ALARM 19

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

const char* DEFAULT_WIFI_SSID = "iPhone";
const char* DEFAULT_WIFI_PASS = "A38523852g";
const char* DEFAULT_MQTT_HOST = "f2bcdee1e5634900a9a6953ad79a121b.s1.eu.hivemq.cloud";
const int   DEFAULT_MQTT_PORT = 8883;
const char* DEFAULT_MQTT_USER = "MSNCHHOME";
const char* DEFAULT_MQTT_PASS = "CloudForge2025";

const char* TOPIC_STATUS = "cloudforge/home/status";
const char* TOPIC_CMD    = "cloudforge/home/cmd";

const char* AP_SSID = "CloudForge_Setup";
const char* AP_PASS = "12345678";

Preferences prefs;
WebServer server(80);
WiFiClientSecure tls;
PubSubClient mqtt(tls);

String confWiFiSsid, confWiFiPass, confMqttHost, confMqttUser, confMqttPass;
int confMqttPort = 8883;
unsigned long lastBlink=0,lastHb=0; uint8_t failCount=0;

String htmlEscape(const String& in){String o;o.reserve(in.length());
 for(char c:in){if(c=='&')o+="&amp;";else if(c=='<')o+="&lt;";else if(c=='>')o+="&gt;";else if(c=='\"')o+="&quot;";else if(c=='\'')o+="&#39;";else o+=c;}return o;}

void oledCenter(const char* l1,const char* l2="",const char* l3=""){oled.clearBuffer();oled.setFont(u8g2_font_unifont_t_chinese2);oled.drawUTF8(0,18,l1);oled.drawUTF8(0,38,l2);oled.drawUTF8(0,58,l3);oled.sendBuffer();}

void bootAnimation(){for(int x=128;x>-170;x-=2){oled.clearBuffer();oled.setFont(u8g2_font_7x13_tf);oled.drawStr(x,18,"C L O U D F O R G E");oled.setFont(u8g2_font_unifont_t_chinese2);oled.drawUTF8(0,58,"雲匠 智慧住宅啟動中...");oled.sendBuffer();delay(12);}for(int i=0;i<3;i++){oled.clearDisplay();delay(120);oled.sendBuffer();delay(120);}}

String buildForm(){String h="<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>CloudForge 設定</title><style>body{font-family:system-ui,Segoe UI,Noto Sans TC,sans-serif;padding:16px}label{display:block;margin:8px 0 4px}input{width:100%;padding:8px;border:1px solid #ddd;border-radius:6px}button{margin-top:12px;padding:10px 16px;border:0;border-radius:6px;background:#2c7be5;color:#fff}.tip{font-size:12px;color:#666}</style></head><body><h2>🌿 CloudForge 雲匠 — 智慧設定</h2><form method='POST' action='/save'>";
 h+="<label>Wi‑Fi 名稱（SSID）</label><input name='ssid' value='"+htmlEscape(confWiFiSsid)+"'>";
 h+="<label>Wi‑Fi 密碼</label><input name='wpass' value='"+htmlEscape(confWiFiPass)+"'>";
 h+="<label>MQTT 主機（HiveMQ）</label><input name='mhost' value='"+htmlEscape(confMqttHost)+"'>";
 h+="<label>MQTT 埠（TLS）</label><input name='mport' value='"+String(confMqttPort)+"'>";
 h+="<label>MQTT 帳號</label><input name='muser' value='"+htmlEscape(confMqttUser)+"'>";
 h+="<label>MQTT 密碼</label><input name='mpass' value='"+htmlEscape(confMqttPass)+"'>";
 h+="<button type='submit'>儲存並重啟</button><p class='tip'>儲存後約 3~5 秒自動重啟並上雲。</p></form></body></html>";return h;}

void startApPortal(){oledCenter("建立設定熱點","CloudForge_Setup","密碼：12345678");WiFi.mode(WIFI_AP);WiFi.softAP(AP_SSID,AP_PASS);
 server.on("/",HTTP_GET,[](){server.send(200,"text/html; charset=utf-8",buildForm());});
 server.on("/save",HTTP_POST,[](){String ssid=server.arg("ssid"),wpass=server.arg("wpass"),mhost=server.arg("mhost"),mport=server.arg("mport"),muser=server.arg("muser"),mpass=server.arg("mpass");
 prefs.begin("cf-home",false);prefs.putString("ssid",ssid);prefs.putString("wpass",wpass);prefs.putString("mhost",mhost);prefs.putInt("mport",mport.toInt());prefs.putString("muser",muser);prefs.putString("mpass",mpass);prefs.putBool("ready",true);prefs.end();
 server.send(200,"text/html; charset=utf-8","<meta charset='utf-8'><p>設定已儲存，3 秒後重啟…</p>");delay(3000);ESP.restart();});
 server.begin();while(true){server.handleClient();delay(10);}}

void loadConfigOrDefault(){prefs.begin("cf-home",true);bool ready=prefs.getBool("ready",false);
 confWiFiSsid=prefs.getString("ssid",DEFAULT_WIFI_SSID);confWiFiPass=prefs.getString("wpass",DEFAULT_WIFI_PASS);
 confMqttHost=prefs.getString("mhost",DEFAULT_MQTT_HOST);confMqttPort=prefs.getInt("mport",DEFAULT_MQTT_PORT);
 confMqttUser=prefs.getString("muser",DEFAULT_MQTT_USER);confMqttPass=prefs.getString("mpass",DEFAULT_MQTT_PASS);prefs.end();}

bool connectWiFiSTA(){WiFi.mode(WIFI_STA);WiFi.begin(confWiFiSsid.c_str(),confWiFiPass.c_str());unsigned long start=millis();
 while(WiFi.status()!=WL_CONNECTED && millis()-start<20000){if(millis()-lastBlink>250){lastBlink=millis();digitalWrite(LED_NET,!digitalRead(LED_NET));}
 oledCenter("連線 Wi‑Fi 中…",confWiFiSsid.c_str()," ");delay(50);}digitalWrite(LED_NET,HIGH);
 if(WiFi.status()==WL_CONNECTED){oledCenter("Wi‑Fi 已連線",WiFi.localIP().toString().c_str()," ");delay(800);return true;}return false;}

void onMqtt(char* topic, byte* payload, unsigned int len){char msg[128]={0};unsigned int n=(len<127)?len:127;memcpy(msg,payload,n);oledCenter("收到雲端指令",topic,msg);}

bool connectMQTT(){tls.setInsecure();mqtt.setServer(confMqttHost.c_str(),confMqttPort);mqtt.setCallback(onMqtt);oledCenter("連線 MQTT 中…",confMqttHost.c_str()," ");
 if(mqtt.connect("CloudForge_Master",confMqttUser.c_str(),confMqttPass.c_str())){mqtt.subscribe(TOPIC_CMD);mqtt.publish(TOPIC_STATUS,"CloudForge Master Online");oledCenter("MQTT 已連線","狀態：Connected"," ");delay(800);return true;}
 char buf[64];snprintf(buf,sizeof(buf),"rc=%d",mqtt.state());oledCenter("MQTT 連線失敗",buf,"10 秒後重試…");delay(10000);return false;}

bool bringOnline(){if(!connectWiFiSTA())return false; if(!connectMQTT())return false; return true;}

void setup(){
 pinMode(LED_NET,OUTPUT);pinMode(LED_WARN,OUTPUT);pinMode(LED_ALERT,OUTPUT);
 pinMode(IN_LOW_ALARM,INPUT_PULLUP);pinMode(IN_HIGH_ALARM,INPUT_PULLUP);
 Serial.begin(115200);delay(50);Wire.begin(SDA_PIN,SCL_PIN);oled.begin();oled.setContrast(200);oled.clearBuffer();
 bootAnimation();loadConfigOrDefault();if(!bringOnline()){startApPortal();}
}

void loop(){
 if(mqtt.connected())mqtt.loop();
 if(millis()-lastHb>10000 && mqtt.connected()){lastHb=millis();mqtt.publish(TOPIC_STATUS,"heartbeat v4.1");}
 float lowVal=analogRead(LVL_LOW)/4095.0f*100.0f, midVal=analogRead(LVL_MID)/4095.0f*100.0f, highVal=analogRead(LVL_HIGH)/4095.0f*100.0f;
 bool lowAlarm=!digitalRead(IN_LOW_ALARM), highAlarm=!digitalRead(IN_HIGH_ALARM);
 digitalWrite(LED_WARN,(lowVal<30.0f)||lowAlarm); digitalWrite(LED_ALERT,(highVal>90.0f)||highAlarm);
 oled.clearBuffer();oled.setFont(u8g2_font_unifont_t_chinese2);oled.drawUTF8(0,12,"🏠 雲匠智慧住宅");oled.drawUTF8(0,28,(String("Wi‑Fi: ")+confWiFiSsid).c_str());oled.drawUTF8(0,44,(String("IP: ")+WiFi.localIP().toString()).c_str());
 oled.setFont(u8g2_font_6x12_tf);oled.drawStr(0,60,mqtt.connected()?"MQTT: Connected":"MQTT: Reconnecting…");oled.sendBuffer();
 if(WiFi.status()!=WL_CONNECTED || !mqtt.connected()){failCount++; if(!bringOnline() && failCount>=3){oledCenter("連線多次失敗","系統將重啟…"," ");delay(1500);ESP.restart();}} else {failCount=0;}
 if(millis()-lastBlink>1000){lastBlink=millis();digitalWrite(LED_NET,!digitalRead(LED_NET));}
 delay(100);
}
