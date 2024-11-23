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

// TODO fake pill hack
int fake_idx = -1;
int real_idx = -1;
int fake_pill = -1;
int real_pill = -1;

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
	setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
	tzset();

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

	// TODO a hack for single pill config
	if (total_pills == 1)
	{
		real_pill = atoi(conf["pills"][0]);
		fake_pill = real_pill / 2;
		fake_idx = 0;
		real_idx = 1;
		if (fake_pill < 6)
		{
			fake_pill += 12;
			fake_idx = 1;
			real_idx = 0;
		}
		total_pills += 1;
		Serial.println("(added fake pill)");
	}

	Serial.println(total_pills);
	pill = (int *)malloc(sizeof(int)*total_pills);
	done = (int *)malloc(sizeof(int)*total_pills);
	hours = (int *)malloc(sizeof(int)*HOUR_GRAN);
	// TODO hadle special case when single pill used
	if (fake_idx >= 0)
	{
		pill[fake_idx] = fake_pill*(HOUR_GRAN/24);
		pill[real_idx] = real_pill*(HOUR_GRAN/24);
	}
	// otherwise proceed as always
	else
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

	char debug_buff[256];
	snprintf(debug_buff, 256, "hour=%d, prev_hour=%d, pill_hour=%d, stamp=%lu, lastOpen=%lu, diff=%lu, fake_idx=%d, real_idx=%d",
		hour, prev_hour, pill_hour, stamp, lastOpen, diff, fake_idx, real_idx);
	if (!((stamp/1000) % 5))
	{
		Serial.println("");
		Serial.println(debug_buff);
	}
	else
		Serial.print("~");

	if (prev_hour != hour)
	{
		prev_hour = hour;
		Serial.println(api_send(ACTION_PING, hour));
	}

	// closed
	if(state == STATE_OPENED && digitalRead(HAL) == STATE_CLOSED)
	{
		state = STATE_CLOSED;
		digitalWrite(RED, LOW);
		Serial.println("closed");
		Serial.println(api_send(ACTION_CLOSE, hour));
	}

	// opened
	if(state != STATE_OPENED && digitalRead(HAL) == STATE_OPENED && millis() > lastOpen + cooldown)
	{
		String res;
		state = STATE_OPENED;
		lastOpen = millis();
		reset_dones();
		done[pill_hour] = 1;
		Serial.println("opened");
		res = api_send(ACTION_OPEN, hour);
		Serial.println(res);
		if (res == "")
			digitalWrite(RED, HIGH);
	}

	// overdue
	if(state == STATE_CLOSED && hour >= pill[pill_hour] && done[pill_hour] == 0 && pill_hour != fake_idx)
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
