#include <Arduino.h>
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

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(WIFISSID, WIFIPSK);

  Serial.print("Waiting for WiFi to connect...");
  while ((WiFiMulti.run() != WL_CONNECTED))
    Serial.print(".");
  Serial.println(" connected");

  setNtp();

  Serial.println(upload_data(CONFURL));
  total_pills = 2;
  pill = (int *)malloc(sizeof(int)*total_pills);
  done = (int *)malloc(sizeof(int)*total_pills);
  pill[0] = 7;
  pill[1] = 20;
  calc_hours(total_pills);
  reset_dones();

  pinMode(HAL, INPUT_PULLUP);
  pinMode(RED, OUTPUT);
  digitalWrite(RED, LOW);

  // initial state is always "pill has been just taken"
  done[hours[get_hour()]] = 1;
  state = STATE_CLOSED;
  lastOpen = millis();

}

void loop()
{
  int hour = get_hour(); // current hour
  int pill_hour = hours[hour]; // which pill idx belongs to this hour
  unsigned long stamp = millis();
  unsigned long diff = stamp - lastOpen;

  // closed
  if(state != STATE_CLOSED && digitalRead(HAL) == STATE_CLOSED)
  {
    state = STATE_CLOSED;
    digitalWrite(RED, LOW);
    Serial.println("closed");
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
    char buff[256];
    snprintf(buff, 256, "%s%d", LOGURL, hour);
    Serial.println(upload_data(buff));
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
