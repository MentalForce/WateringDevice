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

struct WateringEvent
{
    time_t NextEventUnixTime; 
    unsigned long FrequencyInSeconds;
    byte DurationInSeconds;
};

struct WateringEvent *WateringEvents;
int WateringEventsCount = 0;
const byte WaterPumpPin = 7;

void setup()
{
    Serial.begin(9600);

    setSyncProvider(RTC.get);

    pinMode(WaterPumpPin, OUTPUT);
    
    char *initializationMessage = ReadInitializationMessage();
    Serial.print("Parsing input: '");
    Serial.print(initializationMessage);
    Serial.print("'\n");
    
    Serial.print("Parsing complete. Current time is: ");
    PrintDateTime(now());
    Serial.print("\n");
    
    WateringEventsCount = GetEventsCount(initializationMessage);
    WateringEvents = (struct WateringEvent*) malloc(sizeof(WateringEvent) * WateringEventsCount);
    ParseWateringEvents(initializationMessage);
    
    free(initializationMessage);
  
    Serial.print("Events found: ");
    Serial.print(WateringEventsCount);
    Serial.print("\n");
}

void loop() 
{
    time_t currentDateTime = now();

    //INFO
    Serial.print("Current time: ");
    PrintDateTime(currentDateTime);
    Serial.print("\n");
    for(int i = 0; i < WateringEventsCount; i++)
    {
      Serial.print(i);
      Serial.print(" Event will be triggered: ");
      PrintDateTime((WateringEvents + i)->NextEventUnixTime);
      Serial.print(" for '");
      Serial.print((WateringEvents + i)->DurationInSeconds);
      Serial.print("' seconds, with frequency of '");
      Serial.print((WateringEvents + i)->FrequencyInSeconds);
      Serial.print("' seconds.\n");
    }
    //INFO
    
    ProcessEvents(currentDateTime);

    delay(10000);
}

void ProcessEvents(time_t currentUnixTime)
{
    time_t currentTime = currentUnixTime;
    
    for(int i = 0; i < WateringEventsCount; i++)
    {
        if(currentTime >= (WateringEvents + i)->NextEventUnixTime)
        {
            EnableWaterPump((WateringEvents + i)->DurationInSeconds);
            
            (WateringEvents + i)->NextEventUnixTime = (WateringEvents + i)->NextEventUnixTime + (WateringEvents + i)->FrequencyInSeconds;
            
            currentTime = currentTime + (WateringEvents + i)->DurationInSeconds;
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
    const byte maxTransmissionLengh = 200;
    const char endOfTransmissionCharacter = '|';
    char receivedCharacters[maxTransmissionLengh];
    char currentCharacter;
    bool transmissionComplete = false;
    byte index = 0;
    int charactersCount = 0;
    
    while(transmissionComplete == false)
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

void ParseWateringEvents(char *str)
{
    char *s = (char*) malloc(strlen(str)+1);
    char *token;
    strcpy(s, str);

    // Find beginning of the events section.
    s = strstr(s,"E");
    if(s == NULL)
      {
        Serial.print("Events not found.");
        return;
      }
    s++;
    token = strtok(s, "F");

    for(int i = 0; i < WateringEventsCount; i++)
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

        *(WateringEvents + i) = ve;
    }
    
    free(s);
}

void EnableWaterPump(int durationInSeconds)
{
    digitalWrite(WaterPumpPin, HIGH);
    delay(durationInSeconds * 1000);
    digitalWrite(WaterPumpPin, LOW);
}
