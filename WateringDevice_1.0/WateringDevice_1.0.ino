/*
Program has known bug: it sets correct time only starting from second attempt. 
First attempt always sets wrong time.
*/

#include <time.h>

void setup() 
{
    Serial.begin(9600);
      
    const byte maxTransmissionLengh = 100;
    const char endOfLineCharacter = '\n';
    char receivedCharacters[maxTransmissionLengh]; // an array to store the received data from Serial
    char currentCharacter;
    bool isInitialized = false;
    byte index = 0;

    Serial.print("Format example: '2018-08-03 01:11:54'\n");
    
    while(isInitialized == false)
    {
        Serial.print("Waiting for Date/Time initialization.\n");
        
        while (Serial.available() > 0 && isInitialized == false)
        {
            currentCharacter = Serial.read();
            if(currentCharacter != endOfLineCharacter)
            {
                if(index > maxTransmissionLengh - 1)
                {
                    Serial.print("Initialization failure. Date/Time message to long. Try again.\n");
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
                isInitialized = true;
                receivedCharacters[index] = '\0'; 
            }
        }
        delay(1000);
    }
    
    Serial.print("Parsing result.\n");
    struct tm systime;
    char *strtokResult;
    
    strtokResult = strtok(receivedCharacters, "-");
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
    
    Serial.print("Initialization complete with following parameters:");
    char buf[maxTransmissionLengh];
    sprintf(buf, "%d-%d-%d %d:%d:%d\n", systime.tm_year, systime.tm_mon, systime.tm_mday, systime.tm_hour, systime.tm_min, systime.tm_sec);
    Serial.print(buf);
    
    set_system_time(mktime(&systime));

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
    
    char buf[100];
    
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    sprintf(buf, "%d-%d-%d %d:%d:%d\n", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    Serial.print(buf);
}



