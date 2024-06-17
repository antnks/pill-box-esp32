#include "wifi_pass.h"

// more granular hours - every 30 min
unsigned int HOUR_GRAN = 48;
long HOUR = 3600000; //60*60*1000
unsigned int RED = 15;
unsigned int HAL = 13;

int total_pills;
int *pill; // deadline hours
int *done; // pill taken or not
int *hours; // distribution of hours, hours[hour] = pill_idx

const char ACTION_CONFIG[] = "config";
const char ACTION_OPEN[] = "open";
const char ACTION_CLOSE[] = "close";
const char ACTION_PING[] = "ping";

unsigned long cooldown = 3000;
unsigned long open_too_long = 10000;

int STATE_CLOSED = 0;
int STATE_OPENED = 1;
int STATE_OVERDUE = 2;

const char *rootCACertificate PROGMEM = \
	// lets encrypt, ISRG Root X1
	"-----BEGIN CERTIFICATE-----\n" \
	"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\n" \
	"-----END CERTIFICATE-----\n";

String api_send(const char *action, int hour)
{
  char URL[256];
  snprintf(URL, 256, "%s?box=%d&action=%s&h=%d&gran=%d", APIURL, boxid, action, hour, HOUR_GRAN);

  WiFiClientSecure *client = new WiFiClientSecure;
  if(client)
  {
    client->setCACert(rootCACertificate);

    {
      HTTPClient https;
  
      //Serial.print("https begin\n");
      if (https.begin(*client, URL))
      {
        //Serial.print("GET\n");
        int httpCode = https.GET();
  
        if (httpCode > 0)
        {
          //Serial.printf("Code: %d\n", httpCode);
  
          if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
          {
            String payload = https.getString();
            //Serial.println(payload);
            delete client;
            return payload;
          }
        }
        else
        {
          Serial.printf("error: %s\n", https.errorToString(httpCode).c_str());
        }
  
        https.end();
      }
      else
      {
        Serial.printf("Unable to connect\n");
      }
    }
  }
  else
  {
    Serial.println("Unable to create client");
  }

  delete client;
  return "";
}

unsigned int get_hour()
{
  time_t now;
  struct tm timeinfo;
  int granularity;

  time(&now);
  localtime_r(&now, &timeinfo);

  granularity = timeinfo.tm_min / (60/(HOUR_GRAN/24));

  return timeinfo.tm_hour*(HOUR_GRAN/24) + granularity;
}

void setNtp()
{
  configTime(0, 0, "pool.ntp.org");

  Serial.print("Waiting for NTP time sync: ");
  time_t nowSecs = 0;
  while (nowSecs < 8 * 3600 * 2)
  {
    delay(500);
    Serial.print(".");
    yield();
    nowSecs = time(nullptr);
  }

  Serial.println();
  Serial.print("Current hour: ");
  Serial.println(get_hour());
}

// fill array with nearest pill to every hour
void calc_hours(int count)
{
  for (int i=0; i<HOUR_GRAN; i++)
  {
    int nearest = HOUR_GRAN + 1;
    int nearest_idx = count + 1;
    for(int j=0; j<count; j++)
    {
      int diff1 = abs(pill[j] - i);
      int diff2 = HOUR_GRAN - diff1;
      int diff = (diff1 < diff2)?diff1:diff2;
      if(diff < nearest)
      {
        nearest = diff;
        nearest_idx = j;
      }
    }
    hours[i] = nearest_idx;
  }
}

void reset_dones()
{
  for(int i=0;i<total_pills;i++)
    done[i] = 0;
}

void blink1()
{
  digitalWrite(RED, LOW);
  delay(500);
  digitalWrite(RED, HIGH);
}

void blink2()
{
  digitalWrite(RED, LOW);
  delay(100);
  digitalWrite(RED, HIGH);
  delay(100);
  digitalWrite(RED, LOW);
  delay(100);
  digitalWrite(RED, HIGH);
  delay(100);
  digitalWrite(RED, LOW);
  delay(100);
  digitalWrite(RED, HIGH);
}
