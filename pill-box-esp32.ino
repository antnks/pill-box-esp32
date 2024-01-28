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

  // initial state is always "pill has been just taken"
  isMorningDone = morningDone();
  state = STATE_CLOSED;
  lastOpen = millis();

}

void loop()
{
  int hour = get_hour();
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
    isMorningDone = morningDone();
    digitalWrite(RED, HIGH);
    Serial.println("opened");
    Serial.println(upload_data(LOGURL + hour));
  }

  // overdue
  if(state == STATE_CLOSED && (hour >= morning && hour < evening && !isMorningDone ||
                         hour >= evening && hour < 24      &&  isMorningDone))
  {
    state = STATE_OVERDUE;
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

