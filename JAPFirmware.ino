#include <SpeedyStepper.h>

const unsigned long DEFAULT_BAUDRATE = 115200;

const int DIR_PIN = 4;
const int STEP_PIN = 7;

const int UP_BTN_PIN = A0;
const int DOWN_BTN_PIN = A1;

const float STEPS_PER_MM = 400*16/4; //steps per revolution * microstepping / mm per revolution
const float MANUAL_MOVEMENT_MM = 0.1;
const float LOW_SPEED = 2;
const float HIGH_SPEED = 8;
const float LOW_ACCELERATION = 5;
const float HIGH_ACCELERATION = 20;

const int CMD_TIMEOUT = 5000;
const int BUF_LEN = 100;
char cmdBuf[BUF_LEN];

SpeedyStepper stepper;

template<typename T>
void debugPrint(T value)
{
    bool DEBUG_ENABLED = true;
    Serial.println(value);
}


void setSteperLowSpeed()
{
    stepper.setSpeedInMillimetersPerSecond(LOW_SPEED);
    stepper.setAccelerationInMillimetersPerSecondPerSecond(LOW_ACCELERATION);
}

void setSteperHighSpeed()
{
    stepper.setSpeedInMillimetersPerSecond(HIGH_SPEED);
    stepper.setAccelerationInMillimetersPerSecondPerSecond(HIGH_ACCELERATION);
}

void setup()
{
    stepper.connectToPins(STEP_PIN, DIR_PIN);
    stepper.setStepsPerMillimeter(STEPS_PER_MM);
    setSteperHighSpeed();

    pinMode(UP_BTN_PIN, INPUT_PULLUP);
    pinMode(DOWN_BTN_PIN, INPUT_PULLUP);

    Serial.begin(DEFAULT_BAUDRATE);
}

bool isButtonPressed(int btnPin)
{
    return digitalRead(btnPin) == LOW;
}

void processBtnMovement(int btnPin, int direction = 1)
{
    // Try small movements first
    setSteperHighSpeed();
    for(int i=0; i<5; i++)
    {
        stepper.moveRelativeInMillimeters(MANUAL_MOVEMENT_MM * direction);
        delay(300);
        if(!isButtonPressed(btnPin))
            return;
    }

    // Then move at low speed for 3 sec
    setSteperLowSpeed();
    unsigned long startTime = millis();
    while(millis() < startTime + 3000)
    {
        if(stepper.motionComplete())
            stepper.setupRelativeMoveInMillimeters(1000 * direction);
        else
            stepper.processMovement();

        // Check if button released
        if(!isButtonPressed(btnPin))
        {
            stepper.setupStop();
            while(!stepper.processMovement())
                ;
            return;
        }
    }

    // Then move at high speed
    setSteperHighSpeed();
    stepper.setupRelativeMoveInMillimeters(1000 * direction);
    while(isButtonPressed(btnPin))
    {
        if(stepper.motionComplete())
            stepper.setupRelativeMoveInMillimeters(1000 * direction);
        else
            stepper.processMovement();
    }

    // Stop when button released
    stepper.setupStop();
    while(!stepper.processMovement())
        ;
}

void processSerialInput()
{
    // If nothing arrived - get out of here to process buttons
    if(!Serial.available())
        return;

    int idx = 0;
    unsigned long startMS = millis();
    while(true)
    {
        // Check timeout
        if(millis() - startMS > CMD_TIMEOUT)
        {
            debugPrint("Receive command timeout exceeded");
            return;
        }

        // Store chars to the buffer until \n is received
        if(Serial.available())
        {
            char ch = Serial.read();
            cmdBuf[idx++] = ch;

            if(ch == '\n')
                break;
        }
    }

    // Process the received command
    debugPrint("Received command: ");
    debugPrint(cmdBuf);
}

void checkAlive()
{
    static unsigned long lastMS = 0;
    if(millis() - lastMS > 1000)
    {
        lastMS = millis();
        debugPrint("Alive");
    }
}

void loop()
{
    checkAlive();

    if(isButtonPressed(UP_BTN_PIN))
        processBtnMovement(UP_BTN_PIN, 1);
    if(isButtonPressed(DOWN_BTN_PIN))
        processBtnMovement(DOWN_BTN_PIN, -1);

    processSerialInput();
}

