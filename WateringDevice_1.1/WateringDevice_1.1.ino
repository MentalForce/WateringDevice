/*
Message longer than 64bytes should be transmitted by batches
'|' - end of transmission character;

Input format: '2018-08-04 12:48:55Evnt:2018-08-04 12:48:55Freq:1440Drtn:10|'
First timestamp: Initializa current time;
Evnt: Timestamp of the next event;
Freq: Event frequency in minutes;
Drtn: Pump work time in seconds;
*/
 
#include <time.h>

struct WateringEvent
{
    time_t EventDateTime; 
    short FrequencyInMinutes;
    byte DurationInSeconds;
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
    
    struct tm systime;
    char *strtokResult;
    
    strtokResult = strtok(initializationMessage, "-");
    systime.tm_year = atoi(strtokResult) - 1900;
    
    strtokResult = strtok(NULL, "-");
    systime.tm_mon = atoi(strtokResult) - 1;
    
    strtokResult = strtok(NULL, " ");
    systime.tm_mday = atoi(strtokResult);
    
    strtokResult = strtok(NULL, ":");
    systime.tm_hour = atoi(strtokResult);
    
    strtokResult = strtok(NULL, ":");
    systime.tm_min = atoi(strtokResult);
    
    strtokResult = strtok(NULL, ":");
    systime.tm_sec = atoi(strtokResult);
    
    systime.tm_isdst = false;
    
    set_system_time(mktime(&systime));
    Serial.print("Time initialization complete. Current time is: ");
    PrintCurrentDateTime();
    
    WateringEventsCount = GetEventsCount(initializationMessage);
    Serial.print("Events found: ");
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
    delay(5000);
    
    PrintCurrentDateTime();
}

void PrintCurrentDateTime()
{
  time_t t = time(NULL);
  struct tm tm = *localtime(&t);
  
  char buf[100];
  sprintf(buf, "%d-%d-%d %d:%d:%d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  Serial.print(buf);
}

int GetEventsCount(char *str)
{
    int count = 0;
    const char *tmp = str;
    while(tmp = strstr(tmp, "Evnt:"))
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
    
    while(transmissionComplete == false)
    {
        Serial.print("Waiting for initialization.\n");
        
        while (Serial.available() > 0 && transmissionComplete == false)
        {
            currentCharacter = Serial.read();
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
    
    return receivedCharacters;
}