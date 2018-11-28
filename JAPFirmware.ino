#include <SpeedyStepper.h>

const unsigned long DEFAULT_BAUDRATE = 115200;

const int DIR_PIN = 4;
const int STEP_PIN = 7;
const int ENABLE_PIN = 8;

const int UP_BTN_PIN = A0;
const int DOWN_BTN_PIN = A1;

const int UV_LED_PIN = A6;

const float STEPS_PER_MM = 400*16/8; //steps per revolution * microstepping / mm per revolution
const float MANUAL_MOVEMENT_MM = 0.1;
const float LOW_SPEED = 2;
const float HIGH_SPEED = 8;
const float LOW_ACCELERATION = 5;
const float HIGH_ACCELERATION = 20;

const int CMD_TIMEOUT = 5000;
const int BUF_LEN = 100;

SpeedyStepper stepper;

bool relativePositioning = true;

template<typename T>
void debugPrint(T value, bool addNewLine = true)
{
    bool DEBUG_ENABLED = true;
    if(DEBUG_ENABLED)
        if(addNewLine)
            Serial.println(value);
        else
            Serial.print(value);
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
    pinMode(ENABLE_PIN, OUTPUT);

    stepper.connectToPins(STEP_PIN, DIR_PIN);
    stepper.setStepsPerMillimeter(STEPS_PER_MM);
    setSteperHighSpeed();

    pinMode(UP_BTN_PIN, INPUT_PULLUP);
    pinMode(DOWN_BTN_PIN, INPUT_PULLUP);

    pinMode(UV_LED_PIN, OUTPUT);
    digitalWrite(UV_LED_PIN, LOW);

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

int parseInt(const char * buf, char prefix, int value)
{
    const char * ptr = buf;

    while(ptr && *ptr)
    {
        if(*ptr == prefix)
            return atoi(ptr + 1);

        ptr = strchr(ptr,' ') + 1;
    }
    return value;
}

float parseFloat(const char * buf, char prefix, float value)
{
    const char * ptr = buf;

    while(ptr && *ptr)
    {
        if(*ptr == prefix)
            return atof(ptr + 1);

        ptr = strchr(ptr,' ') + 1;
    }
    return value;
}


// TODO: These commands were supported by original JAP firmware
// M3 - Servo position (not going to be used)
// M7/M9 - Coolant On/off ???
// M106/M107 - Cooler On/Off  (LED????)
// M245/M246 - Cooler On/Off ???

// These can be also sent by nanodlp
// M84
// M650 T20
// M653
// M654


void processMotorOnCmd() //M17
{
    digitalWrite(ENABLE_PIN, LOW);
}

void processMotorOffCmd() //M18
{
    digitalWrite(ENABLE_PIN, HIGH);
}

void processLEDOnCmd() // M3
{
    digitalWrite(UV_LED_PIN, HIGH);
}

void processLEDOffCmd() // M5
{
    digitalWrite(UV_LED_PIN, LOW);
}


void processMoveCmd(float position, float speed)
{
    if(speed != 0)
        stepper.setSpeedInMillimetersPerSecond(speed / 60);

    if(relativePositioning)
        stepper.moveRelativeInMillimeters(position);
    else
        stepper.moveToPositionInMillimeters(position);
}

void processPauseCmd(int duration)
{
    delay(duration);
}

bool parseGCommand(const char * cmd)
{
    int cmdID = parseInt(cmd, 'G', 0);
    switch(cmdID)
    {
        case 1: // G1 Move
        {
            float len = parseFloat(cmd, 'Z', 0);
            float speed = parseFloat(cmd, 'F', 0);
            processMotorOnCmd();
            processMoveCmd(len, speed);

            // NanoDLP waits for a confirmation that movement was completed
            Serial.write("Z_move_comp\n");
            return true;
        }
        case 4: // G4 Pause
        {
            int duration = parseInt(cmd, 'P', 0);
            processPauseCmd(duration);
            return true;
        }

        case 90: // G90 - Set Absolute Positioning
            relativePositioning = false;
            return true;

        case 91: // G91 - Set Relative Positioning
            relativePositioning = true;
            return true;

    }

    return false;
}

bool parseMCommand(const char * cmd)
{
    int cmdID = parseInt(cmd, 'M', 0);
    switch(cmdID)
    {
    case 3: // M3 - UV LED On
        processLEDOnCmd();
        return true;

    case 5: // M5 - UV LED Off
        processLEDOffCmd();
        return true;

    case 17: // M17 - Motor on
        processMotorOnCmd();
        return true;

    case 18: // M18 - Motor off
        processMotorOffCmd();
        return true;

    case 114: // M114 - Get current position
        float pos = stepper.getCurrentPositionInMillimeters();
        Serial.print("Z:");
        Serial.print(pos, 2);
        Serial.write('\n');
        return true;
    }

    return false;
}

bool parseCommand(const char * cmd)
{
    switch(*cmd)
    {
    case 'G':
        return parseGCommand(cmd);

    case 'M':
        return parseMCommand(cmd);

    default:
        break;
    }

    return false;
}

void processSerialInput()
{
    // If nothing arrived - get out of here to process buttons
    if(!Serial.available())
        return;

    int idx = 0;
    unsigned long startMS = millis();
    char cmdBuf[BUF_LEN];

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
            cmdBuf[idx] = ch;

            // Ignore \r's
            if(ch == '\r')
                continue;

            // Stop receiving more symbols on \n
            if(ch == '\n')
            {
                cmdBuf[idx] = 0;
                break;
            }

            idx++;
        }
    }

    // Swallow empty lines
    if(cmdBuf[0] == '\0')
        return;

    // Process the received command
    if(parseCommand(cmdBuf))
    {
        Serial.write("ok\n");
    }
    else
    {
        Serial.print("Invalid or unsupported command: ");
        Serial.print(cmdBuf);
        Serial.write('\n');
    }
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
//    checkAlive();

    if(isButtonPressed(UP_BTN_PIN))
        processBtnMovement(UP_BTN_PIN, 1);
    if(isButtonPressed(DOWN_BTN_PIN))
        processBtnMovement(DOWN_BTN_PIN, -1);

    processSerialInput();
}

