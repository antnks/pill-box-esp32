#include <Arduino.h>
#include <Arduino_JSON.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "config.h"

unsigned long lastOpen;
int state;
WiFiMulti WiFiMulti;

void setup()
{
	Serial.begin(9600);
	Serial.println();

	pinMode(HAL, INPUT_PULLUP);
	pinMode(RED, OUTPUT);

	WiFi.mode(WIFI_STA);
	WiFiMulti.addAP(WIFISSID, WIFIPSK);

	Serial.print("Waiting for WiFi to connect...");
	while ((WiFiMulti.run() != WL_CONNECTED))
		Serial.print(".");
	Serial.println(" connected");

	blink1();
	setNtp();

	JSONVar conf = JSON.parse(api_send(ACTION_CONFIG, -1));
	if (JSON.typeof(conf) == "undefined" || JSON.typeof(conf["pills"]) != "array")
	{
		Serial.println("Config load failed");
		return;
	}

	blink2();

	Serial.println(conf["pills"]);
	Serial.println("Total pills:");
	total_pills = conf["pills"].length();
	Serial.println(total_pills);
	pill = (int *)malloc(sizeof(int)*total_pills);
	done = (int *)malloc(sizeof(int)*total_pills);
	hours = (int *)malloc(sizeof(int)*HOUR_GRAN);
	for(int i=0; i<total_pills; i++)
		pill[i] = atoi(conf["pills"][i])*(HOUR_GRAN/24);
	calc_hours(total_pills);
	reset_dones();

	// initial state is always "pill has been just taken"
	done[hours[get_hour()]] = 1;
	state = STATE_CLOSED;
	lastOpen = millis();

	digitalWrite(RED, LOW);
}

int prev_hour = -1;

void loop()
{
	if (!total_pills)
		return;

	int hour = get_hour(); // current hour
	int pill_hour = hours[hour]; // which pill idx belongs to this hour
	unsigned long stamp = millis();
	unsigned long diff = stamp - lastOpen;

	if (prev_hour != hour)
	{
		prev_hour = hour;
		setNtp();
		Serial.println(api_send(ACTION_PING, hour));
	}

	// closed
	if(state != STATE_CLOSED && digitalRead(HAL) == STATE_CLOSED)
	{
		state = STATE_CLOSED;
		digitalWrite(RED, LOW);
		Serial.println("closed");
		Serial.println(api_send(ACTION_CLOSE, hour));
	}

	// opened
	if(state != STATE_OPENED && digitalRead(HAL) == STATE_OPENED && millis() > lastOpen + cooldown)
	{
		state = STATE_OPENED;
		lastOpen = millis();
		reset_dones();
		done[pill_hour] = 1;
		digitalWrite(RED, HIGH);
		Serial.println("opened");
		Serial.println(api_send(ACTION_OPEN, hour));
	}

	// overdue
	if(state == STATE_CLOSED && hour >= pill[pill_hour] && done[pill_hour] == 0)
	{
		state = STATE_OVERDUE;
		Serial.println("Overdue:");
		Serial.println(pill[pill_hour]);
	}

	// blink if open for too long
	if(state == STATE_OPENED && digitalRead(HAL) == STATE_OPENED && millis() > lastOpen + open_too_long)
	{
		blink1();
	}

	// blink even more if overdue
	if(state == STATE_OVERDUE)
	{
		blink2();
	}

	delay(500);
}
