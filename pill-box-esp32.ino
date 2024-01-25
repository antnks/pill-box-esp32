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
  morning = 7;
  evening = 20;

  pinMode(HAL, INPUT_PULLUP);
  pinMode(RED, OUTPUT);
  digitalWrite(RED, LOW);
}

void loop()
{
  // closed
  if(state == OPENED && digitalRead(HAL) == CLOSED)
  {
    state = CLOSED;
    digitalWrite(RED, LOW);
    Serial.println("closed");
  }

  // opened
  if(state == CLOSED && digitalRead(HAL) == OPENED && millis() > lastOpen + cooldown)
  {
    state = OPENED;
    lastOpen = millis();
    digitalWrite(RED, HIGH);
    Serial.println("opened");
    Serial.println(upload_data(LOGURL));
  }

  // overdue
  int hour = get_hour();
  unsigned long stamp = millis();
  long diff = stamp - lastOpen;
  if(state == CLOSED && (hour >= morning && hour < evening && diff > night_pill_interval ||
                         hour >= evening && hour < 24      && diff > day_pill_interval))
  {
    state = OVERDUE;
  }

  // blink if open for too long
  if(state == OPENED && digitalRead(HAL) == OPENED && millis() > lastOpen + open_too_long)
  {
    blink1();
  }

  // blink even more if overdue
  if(state == OVERDUE)
  {
    blink2();
  }

  delay(500);
}
