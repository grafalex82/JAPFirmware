#include <SpeedyStepper.h>


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

SpeedyStepper stepper;

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
    stepper.setSpeedInMillimetersPerSecond(HIGH_SPEED);
    stepper.setAccelerationInMillimetersPerSecondPerSecond(HIGH_ACCELERATION);

    pinMode(UP_BTN_PIN, INPUT_PULLUP);
    pinMode(DOWN_BTN_PIN, INPUT_PULLUP);
}

void processBtnMovement(int btnPin, int direction = 1)
{
    // Try small movements first
    setSteperHighSpeed();
    for(int i=0; i<5; i++)
    {
        stepper.moveRelativeInMillimeters(MANUAL_MOVEMENT_MM * direction);
        delay(300);
        if(digitalRead(btnPin) != LOW)
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
        if(digitalRead(btnPin) != LOW)
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
    while(digitalRead(btnPin) == LOW)
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

void loop()
{
//    stepper.moveRelativeInSteps(50000);
//    delay(1000);
//    stepper.moveRelativeInSteps(-50000);
//    delay(1000);

    if(digitalRead(UP_BTN_PIN) == LOW)
        processBtnMovement(UP_BTN_PIN, 1);
    if(digitalRead(DOWN_BTN_PIN) == LOW)
        processBtnMovement(DOWN_BTN_PIN, -1);
}

