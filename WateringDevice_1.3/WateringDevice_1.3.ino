/*
  Message longer than 64bytes should be transmitted by batches
  
  Input format: 'E2018-09-09 19:19:19F1440D10|'
  'E' - Timestamp of the next event;
  'F' - Event frequency in minutes;
  'D' - Pump work time in seconds (0-255);
  '|' - end of transmission character;
*/

#include <time.h>
#include <DS3232RTC.h>  
#include <EEPROM.h>
#include "LowPower.h"

struct WateringEvent
{
    time_t NextEventUnixTime; 
    unsigned long FrequencyInSeconds;
    byte DurationInSeconds;
};

const byte WaterPumpPin = 9;
const byte RTCPin = 10;
const byte WateringEventsStartEepromAddress = 1; // Zero byte is keeping data of events count.

void setup()
{
    Serial.begin(9600);

    pinMode(WaterPumpPin, OUTPUT);
    pinMode(RTCPin, OUTPUT);
    
    // disable pull-up on I2C to save power
    pinMode(A4, INPUT);
    pinMode(A5, INPUT);
    
    char *initializationMessage = ReadInitializationMessage();
    byte wateringEventsCount = GetEventsCount(initializationMessage);

    if(wateringEventsCount > 0)
    {
        Serial.print("Events found: ");
        Serial.print(wateringEventsCount);
        Serial.print("\n");

        SaveWateringEventsCount(wateringEventsCount);
        ParseAndSaveWateringEvents(initializationMessage, wateringEventsCount);
    }
    
    free(initializationMessage);
}

void loop() 
{
    digitalWrite(RTCPin, HIGH);
    time_t currentDateTime = RTC.get();
    digitalWrite(RTCPin, LOW);
    
    //INFO
    Serial.print("Current time: ");
    PrintDateTime(currentDateTime);
    Serial.print("\n");
    
    byte wateringEventsCount = GetWateringEventsCount();
    struct WateringEvent *wateringEvents = GetWateringEvents(wateringEventsCount);
    for(int i = 0; i < wateringEventsCount; i++)
    {
      Serial.print(i);
      Serial.print(" Event will be triggered: ");
      PrintDateTime((wateringEvents + i)->NextEventUnixTime);
      Serial.print(" for '");
      Serial.print((wateringEvents + i)->DurationInSeconds);
      Serial.print("' seconds, with frequency of '");
      Serial.print((wateringEvents + i)->FrequencyInSeconds);
      Serial.print("' seconds.\n");
    }
    //INFO
    
    ProcessEvents(wateringEvents, wateringEventsCount, currentDateTime);
    free(wateringEvents);
    
    // Wait some time to finish UART transmission before sleep
    delay(50);
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
}

void ProcessEvents(struct WateringEvent *wateringEvents, byte wateringEventsCount, time_t currentUnixTime)
{
    time_t currentTime = currentUnixTime;
    
    for(int i = 0; i < wateringEventsCount; i++)
    {
        if(currentTime >= (wateringEvents + i)->NextEventUnixTime)
        {
            currentTime += (wateringEvents + i)->DurationInSeconds;
            
            EnableWaterPump((wateringEvents + i)->DurationInSeconds);
                        
            (wateringEvents + i)->NextEventUnixTime = (wateringEvents + i)->NextEventUnixTime + (wateringEvents + i)->FrequencyInSeconds;
            
            int eepromAddress = sizeof(WateringEvent) * i + WateringEventsStartEepromAddress + 1;
            EEPROM_Write(eepromAddress, (wateringEvents + i));
        }
    }
}

void PrintDateTime(time_t t)
{
  tmElements_t tm;
  breakTime(t, tm);
  
  char buf[100];
  sprintf(buf, "%d-%02d-%02d %02d:%02d:%02d", tm.Year + 1970, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);
  Serial.print(buf);
}

int GetEventsCount(char *str)
{
    int count = 0;
    const char *tmp = str;
    while(tmp = strstr(tmp, "E"))
    {
        count++;
        tmp++;
    }
    
    return count;
}

char* ReadInitializationMessage()
{
    const byte initializationAttempsCount = 10;
    const byte maxTransmissionLengh = 200;
    const char endOfTransmissionCharacter = '|';
    char receivedCharacters[maxTransmissionLengh];
    char currentCharacter;
    bool transmissionComplete = false;
    byte index = 0;
    byte attemptIndex = 0;
    int charactersCount = 0;
    
    while(transmissionComplete == false || attemptIndex < initializationAttempsCount)
    {
        Serial.print("Waiting for initialization.\n");
        
        while (Serial.available() > 0 && transmissionComplete == false)
        {
            currentCharacter = Serial.read();
            charactersCount++;
            
            if(currentCharacter != endOfTransmissionCharacter)
            {
                if(index > maxTransmissionLengh - 1)
                {
                    Serial.print("Initialization failure. Message to long. Try again.\n");
                    for(byte i = 0; i < maxTransmissionLengh; i++)
                    {
                        receivedCharacters[i] = '\0';
                    }
                    index = 0;
                }
                else
                {
                    receivedCharacters[index] = currentCharacter;
                    index++;
                }
            }
            else
            {
                transmissionComplete = true;
                receivedCharacters[index] = '\0'; 
            }
        }

        attemptIndex++;
        delay(1000);
    }
    
    char *message = (char*) malloc(charactersCount);
    strcpy(message, receivedCharacters);
    return message;
}

tmElements_t ParseSystime(char *str)
{
    char *s = (char*) malloc(strlen(str) + 1);
    strcpy(s, str);
    
    tmElements_t systime;
    char *strtokResult, *tmp;
    
    strtokResult = strtok_r(s, "-", &tmp);
    systime.Year = atoi(strtokResult) - 1970;
    
    strtokResult = strtok_r(NULL, "-", &tmp);
    systime.Month = atoi(strtokResult);
    
    strtokResult = strtok_r(NULL, " ", &tmp);
    systime.Day = atoi(strtokResult);
    
    strtokResult = strtok_r(NULL, ":", &tmp);
    systime.Hour = atoi(strtokResult);
    
    strtokResult = strtok_r(NULL, ":", &tmp);
    systime.Minute = atoi(strtokResult);
    
    strtokResult = strtok_r(NULL, ":", &tmp);
    systime.Second = atoi(strtokResult);
    
    free(s);
    return systime;
}

void ParseAndSaveWateringEvents(char *str, byte wateringEventsCount)
{
    char *s = (char*) malloc(strlen(str)+1);
    strcpy(s, str);
    
    // Find beginning of the events section.
    s = strstr(s,"E");
    if(s == NULL)
      {
        Serial.print("Events not found.");
        return;
      }
    s++;

    char *token = strtok(s, "F");
    int eepromAddress = WateringEventsStartEepromAddress;
    for(int i = 0; i < wateringEventsCount; i++)
    {
        struct WateringEvent ve;

        if( i != 0)
          token = strtok(NULL, "F");    
            
        tmElements_t t = ParseSystime(token);
        ve.NextEventUnixTime = makeTime(t);

        char *p;
        token = strtok(NULL, "D");
        ve.FrequencyInSeconds = strtoul(token, &p, 10);
     
        token = strtok(NULL, "E");
        ve.DurationInSeconds = atoi(token);

        eepromAddress += EEPROM_Write(eepromAddress, ve) + 1;
    }
    
    free(s);
}

void EnableWaterPump(int durationInSeconds)
{
    //digitalWrite(WaterPumpPin, HIGH);
    //delay(durationInSeconds * 1000);
    //digitalWrite(WaterPumpPin, LOW);
}

template <class T> int EEPROM_Write(int ee, const T& value)
{
    const byte* p = (const byte*)(const void*)&value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
          EEPROM.write(ee++, *p++);
    return i;
}

template <class T> int EEPROM_Read(int ee, T& value)
{
    byte* p = (byte*)(void*)&value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
          *p++ = EEPROM.read(ee++);
    return i;
}

byte GetWateringEventsCount()
{
    byte ec;
    EEPROM_Read(0, ec);
    return ec;
}

void SaveWateringEventsCount(byte wateringEventsCount)
{
    EEPROM_Write(0, wateringEventsCount);
}

struct WateringEvent* GetWateringEvents(byte wateringEventsCount)
{
    struct WateringEvent *events = (struct WateringEvent*) malloc(sizeof(WateringEvent) * wateringEventsCount);
    int eepromAddress = WateringEventsStartEepromAddress;
    for(byte i = 0; i < wateringEventsCount; i++)
    {
        struct WateringEvent we;
        eepromAddress += EEPROM_Read(eepromAddress, we) + 1;
        *(events + i) = we;
    }
    return events;
}
