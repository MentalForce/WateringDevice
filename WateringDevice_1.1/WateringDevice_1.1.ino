/*
Message longer than 64bytes should be transmitted by batches

Input format: '2018-08-08 08:08:08E2018-09-09 19:19:19F1440D10|'
First timestamp: Initializa current time;
'E' - Timestamp of the next event;
'F' - Event frequency in minutes;
'D' - Pump work time in seconds;
'|' - end of transmission character;
*/
 
#include <time.h>

struct WateringEvent
{
    time_t EventUnixTime; 
    int FrequencyInSeconds;
    int DurationInSeconds;
};

struct WateringEvent *WateringEvents;
int WateringEventsCount = 0;

void setup() 
{
    Serial.begin(9600);
      
    char *initializationMessage = ReadInitializationMessage();
    
    Serial.print("Parsing input: '");
    Serial.print(initializationMessage);
    Serial.print("'\n");
    
    struct tm systime = ParseSystime(initializationMessage);

    time_t t = mktime(&systime);
    set_system_time(t);
    Serial.print("Time initialization complete. Current time is: ");
    PrintDateTime(t);
    
    WateringEventsCount = GetEventsCount(initializationMessage);
    WateringEvents = (struct WateringEvent*) malloc(sizeof(WateringEvent) * WateringEventsCount);
    ParseWateringEvents(initializationMessage);
    
    free(initializationMessage);
  
    Serial.print("\nEvents found: ");
    Serial.print(WateringEventsCount);
    Serial.print("\n");
    
    // Timer setup
    cli();//stop interrupts
    TCCR1A = 0;// set entire TCCR1A register to 0
    TCCR1B = 0;// same for TCCR1B
    TCNT1  = 0;//initialize counter value to 0
    // set compare match register for 1hz increments
    OCR1A = 15624;// = (16*10^6) / (1*1024) - 1 (must be <65536)
    // turn on CTC mode
    TCCR1B |= (1 << WGM12);
    // Set CS10 and CS12 bits for 1024 prescaler
    TCCR1B |= (1 << CS12) | (1 << CS10);  
    // enable timer compare interrupt
    TIMSK1 |= (1 << OCIE1A);
    sei();//allow interrupts
}

ISR(TIMER1_COMPA_vect)
{
  system_tick();
}

void loop() 
{
    time_t currentDateTime = time(NULL);

        //DELETE
        Serial.print("Current dateTime: ");
        PrintDateTime(currentDateTime);
        Serial.print("\n");
        //DELETE
    
    ProcessEvents(currentDateTime);

    delay(5000);
}

void ProcessEvents(time_t currentUnixTime)
{
    time_t currentTime = currentUnixTime;
    
    for(int i = 0; i < WateringEventsCount; i++)
    {
        if(currentTime >= (WateringEvents + i)->EventUnixTime)
        {
            //DELETE
            PrintDateTime(time(NULL));
            Serial.print(": ");
            Serial.print(i);
            Serial.print(" event started.\n");
            //DELETE

            //Turn on water pump
            delay((WateringEvents + i)->DurationInSeconds * 1000);
            //Turn off water pump

            (WateringEvents + i)->EventUnixTime = 
                (WateringEvents + i)->EventUnixTime +
                (WateringEvents + i)->FrequencyInSeconds +
                (WateringEvents + i)->DurationInSeconds;
            
            currentTime = currentTime + (WateringEvents + i)->DurationInSeconds;
            
            //DELETE
            PrintDateTime(time(NULL));
            Serial.print(": ");
            Serial.print(i);
            Serial.print(" event completed. Next event datetime: ");
            PrintDateTime((WateringEvents + i)->EventUnixTime);
            Serial.print("\n");
            //DELETE
        }
    }
}

void PrintDateTime(time_t t)
{
  struct tm tm = *localtime(&t);
  
  char buf[100];
  sprintf(buf, "%d-%d-%d %d:%d:%d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
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

tm ParseSystime(char *str)
{
    char *s = (char*) malloc(strlen(str) + 1);
    strcpy(s, str);
    
    struct tm systime;
    char *strtokResult, *tmp;
    
    strtokResult = strtok_r(s, "-", &tmp);
    systime.tm_year = atoi(strtokResult) - 1900;
    
    strtokResult = strtok_r(NULL, "-", &tmp);
    systime.tm_mon = atoi(strtokResult) - 1;
    
    strtokResult = strtok_r(NULL, " ", &tmp);
    systime.tm_mday = atoi(strtokResult);
    
    strtokResult = strtok_r(NULL, ":", &tmp);
    systime.tm_hour = atoi(strtokResult);
    
    strtokResult = strtok_r(NULL, ":", &tmp);
    systime.tm_min = atoi(strtokResult);
    
    strtokResult = strtok_r(NULL, ":", &tmp);
    systime.tm_sec = atoi(strtokResult);
    
    systime.tm_isdst = false;
    
    free(s);
    return systime;
}

void ParseWateringEvents(char *str)
{
    char *s = (char*) malloc(strlen(str)+1);
    strcpy(s, str);

    // Rewind current time initializer. Find beginning of the events section.
    strtok(s, "E");
    
    for(int i = 0; i < WateringEventsCount; i++)
    {
        struct WateringEvent ve;
        char *token;
        
        token = strtok(NULL, "F");    
        struct tm t = ParseSystime(token);
        ve.EventUnixTime = mktime(&t);
    
        token = strtok(NULL, "D");     
        ve.FrequencyInSeconds = atoi(token);
        
        token = strtok(NULL, "E");
        ve.DurationInSeconds = atoi(token);

        *(WateringEvents + i) = ve;
    }
    
    free(s);
}
