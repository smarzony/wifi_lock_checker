#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>

#define BUTTON_DELAY 300
#define READ_SENSOR_PERIOS_S 120
#define READ_SENSOR_PERIOS_S_OTA 5
#define REED_SWITCH_PIN D7
#define OTA_SWITCH_PIN D5
#define BATTERY_PIN A0

ESP8266WebServer server(80);
const char* ssid = "smarzony";
const char* password = "metalisallwhatineed";
const char* host = "192.168.1.77";
const int port = 8080;
String idx_reed_switch = "30";
String idx_reed_switch_battery = "32";

bool reedSwitchState, OTAAllow;
String chD7_state = "";
int adc_value, voltage_value, battery_percentage;

void setup() {
	Serial.begin(115200);
	Serial.println("Booting");
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	while (WiFi.waitForConnectResult() != WL_CONNECTED) {
		Serial.println("Connection Failed! Rebooting...");
		delay(5000);
		ESP.restart();
	}

	// Port defaults to 8266
	// ArduinoOTA.setPort(8266);

	// Hostname defaults to esp8266-[ChipID]
	ArduinoOTA.setHostname("NodeMCU V3 with Domoticz");

	// No authentication by default
	// ArduinoOTA.setPassword("admin");

	// Password can be set with it's md5 value as well
	// MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
	// ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

	ArduinoOTA.onStart([]() {
		String type;
		if (ArduinoOTA.getCommand() == U_FLASH)
			type = "sketch";
		else // U_SPIFFS
			type = "filesystem";

		// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
		Serial.println("Start updating " + type);
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
	Serial.println("Ready");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	pinMode(REED_SWITCH_PIN, INPUT_PULLUP);
	pinMode(D5, INPUT_PULLUP);
	pinMode(BATTERY_PIN, INPUT);



	//------------------ Czytaj czujnik i wyœlij stan
	readStateSendState();
	OTAAllow = 1;// digitalRead(OTA_SWITCH_PIN);

	//-------------- Jeœli switch jest w pozycji ota, ponimiñ deepSleep
	if (OTAAllow == 0)
	{
		Serial.println("---- Entering Sleep mode ----");
		ESP.deepSleep(READ_SENSOR_PERIOS_S * 1000000);
		ESP.reset();
	}
	else
	{
		Serial.println("---- Entering OTA mode ----");
		server.on("/", handleRootPath);		 // ustawienie przerwania
		server.begin();
		server.handleClient();
	}
}

void loop() {
	ArduinoOTA.handle();
	server.handleClient();
	unsigned long currentMillis = millis();
	OTAAllow = 1; //digitalRead(OTA_SWITCH_PIN);

	if ((currentMillis % 2000) == 0)
	{
		Serial.print(".");
		delay(1);
	}


	if ( (currentMillis % (READ_SENSOR_PERIOS_S_OTA * 1000) ) < 1)
		readStateSendState();

	if (OTAAllow == 0)
	{
		//ESP.deepSleep(3e6); // 20e6 is 20 microseconds
		Serial.println("\n---- Leaving OTA mode ----");
		ESP.deepSleep(READ_SENSOR_PERIOS_S * 1000000);
	}
}

void readStateSendState()
{
	reedSwitchState = digitalRead(REED_SWITCH_PIN);
	adc_value = analogRead(BATTERY_PIN);
	voltage_value = map(adc_value, 637, 665, 3780, 3960);
	battery_percentage = map(voltage_value, 2500, 4200, 0, 100);
	if (reedSwitchState)
		chD7_state = "Off";
	else
		chD7_state = "On";

	// 665 - 3.96
	// 637 - 3.78


	Serial.print("Sensor state: ");
	Serial.println(reedSwitchState);
	String url = "/json.htm?type=command&param=switchlight&idx=";
	url += idx_reed_switch;
	url += "&switchcmd=";
	url += chD7_state;

	//sendToDomoticz(url);

	delay(10);

	Serial.print("Sensor battery: ");
	Serial.println(battery_percentage);
	url = "/json.htm?type=command&param=udevice&idx=";
	url += idx_reed_switch_battery;
	url += "&nvalue=0&svalue=";
	url += (String)battery_percentage;

	//sendToDomoticz(url);
}

void sendToDomoticz(String url)
{
	HTTPClient http;
	Serial.print("\nconnecting to ");
	Serial.println(host);
	Serial.print("Requesting URL: ");
	Serial.println(url);
	http.begin(host, port, url);
	int httpCode = http.GET();
	if (httpCode) {
		//if (httpCode == 200) {
			String payload = http.getString();
			Serial.println("Domoticz response ");
			Serial.println(payload);
		//}
	}
	Serial.println("closing connection");
	http.end();
	Serial.print("Free mem: ");
	Serial.print(ESP.getFreeHeap());
	Serial.println(" B");
}