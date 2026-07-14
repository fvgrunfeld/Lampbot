//====================================================
// LINE FOLLOWER + VL53L3CX OBSTACLE STOP
//====================================================

#include <Wire.h>
#include <vl53lx_class.h>

//====================================================
// VL53L3CX SETTINGS
//====================================================

#define DEV_I2C Wire
#define XSHUT_PIN_1 0
#define TOF_INTERRUPT_PIN_1 2
#define STOP_PIN 26
#define STOP_DISTANCE_MM 300
#define RESUME_DISTANCE_MM 350


VL53LX sensor_vl53lx_sat(&DEV_I2C, XSHUT_PIN);

volatile bool tofReady = false;
bool obstacleStopped = false;

void tofInterrupt()
{
    tofReady = true;
}

//====================================================
// LINE FOLLOWER SETTINGS
//====================================================

const int NUM_SENSORS = 7;

const int M1_STOP = 64;
const int M2_STOP = 192;

const int BASE_SPEED = 8;
const float Kp = 0.08;

bool INVERT_STEERING = false;
bool INVERT_MOTORS = true;

const unsigned long CONTROL_INTERVAL = 20;
const unsigned long PRINT_INTERVAL = 500;
const unsigned long TOF_PRINT_INTERVAL = 500;
unsigned long lastTofPrint = 0;
struct Sensor
{
    int pin;
    int raw;
    int signal;
    int weight;
};

Sensor sensors[NUM_SENSORS] =
{
    {38,0,0,-300},
    {39,0,0,-200},
    {40,0,0,-100},
    {41,0,0,0},
    {15,0,0,100},
    {16,0,0,200},
    {17,0,0,300}
};

unsigned long lastControl = 0;
unsigned long lastPrint = 0;

int lastPosition = 0;

//====================================================
// SETUP
//====================================================

void setup()
{
    Serial.begin(230400);
    Serial6.begin(115200);

    for(int i=0;i<NUM_SENSORS;i++)
    {
        pinMode(sensors[i].pin, INPUT);
    }

    pinMode(STOP_PIN, OUTPUT);
    digitalWrite(STOP_PIN, HIGH);

    pinMode(TOF_INTERRUPT_PIN, INPUT_PULLUP);

    attachInterrupt(
        TOF_INTERRUPT_PIN,
        tofInterrupt,
        FALLING
    );

    DEV_I2C.begin();

    sensor_vl53lx_sat.begin();

    sensor_vl53lx_sat.VL53LX_Off();

    VL53LX_Error status;

    status = sensor_vl53lx_sat.InitSensor(0x12);

    if(status)
    {
        Serial.println("VL53L3CX INIT FAILED");
        while(1);
    }

    sensor_vl53lx_sat.VL53LX_ClearInterruptAndStartMeasurement();

    Serial.println("=== LINE FOLLOWER + TOF START ===");
}

//====================================================
// LOOP
//====================================================

void loop()
{
    unsigned long now = millis();

    //================================================
    // 1. CHECK TOF SENSOR
    //================================================

    if(tofReady)
    {
        tofReady = false;

        VL53LX_MultiRangingData_t data;

        int status = sensor_vl53lx_sat.VL53LX_GetMultiRangingData(&data);

        if(status == 0 && data.NumberOfObjectsFound > 0)
        {
            int distance = data.RangeData[0].RangeMilliMeter;

            if(now - lastTofPrint >= TOF_PRINT_INTERVAL)
{
                lastTofPrint = now;

                Serial.print("TOF Distance: ");
                Serial.print(distance);
                Serial.println(" mm");
}

            if(!obstacleStopped && distance < STOP_DISTANCE_MM)
            {
                obstacleStopped = true;
                digitalWrite(STOP_PIN, LOW);

                Serial.println("!!! OBSTACLE STOP !!!");
            }

            if(obstacleStopped && distance > RESUME_DISTANCE_MM)
            {
                obstacleStopped = false;
                digitalWrite(STOP_PIN, HIGH);

                Serial.println("CLEAR - RESUME");
            }
        }

        sensor_vl53lx_sat.VL53LX_ClearInterruptAndStartMeasurement();
    }

    //================================================
    // 2. LINE FOLLOWER
    //================================================

    if(now - lastControl >= CONTROL_INTERVAL)
    {
        lastControl = now;

        if(obstacleStopped)
        {
            Serial6.write((uint8_t)M1_STOP);
            Serial6.write((uint8_t)M2_STOP);
            return;
        }

        int minimum = 1023;
        int maximum = 0;

        for(int i=0;i<NUM_SENSORS;i++)
        {
            sensors[i].raw = analogRead(sensors[i].pin);

            if(sensors[i].raw < minimum)
                minimum = sensors[i].raw;

            if(sensors[i].raw > maximum)
                maximum = sensors[i].raw;
        }

        int range = maximum - minimum;

        if(range < 20)
            range = 20;

        for(int i=0;i<NUM_SENSORS;i++)
        {
            sensors[i].signal =
            ((sensors[i].raw - minimum) * 100) / range;

            sensors[i].signal = constrain(
                sensors[i].signal,
                0,
                100
            );
        }

        long weightedSum = 0;
        long signalSum = 0;

        for(int i=0;i<NUM_SENSORS;i++)
        {
            weightedSum += (long)sensors[i].signal * sensors[i].weight;
            signalSum += sensors[i].signal;
        }

        int position;

        if(signalSum > 10)
            position = weightedSum / signalSum;
        else
            position = lastPosition;

        lastPosition = position;

        int correction = (int)(Kp * position);

        correction = constrain(
            correction,
            -20,
            20
        );

        if(INVERT_STEERING)
            correction = -correction;

        int m1 = M1_STOP + BASE_SPEED + correction;
        int m2 = M2_STOP + BASE_SPEED - correction;

        m1 = constrain(m1,1,127);
        m2 = constrain(m2,128,254);

        if(INVERT_MOTORS)
        {
            m2 = M2_STOP - (m2 - M2_STOP);
        }

        Serial6.write((uint8_t)m1);
        Serial6.write((uint8_t)m2);

        if(now-lastPrint >= PRINT_INTERVAL)
        {
            lastPrint = now;

            Serial.print("POS:");
            Serial.print(position);

            Serial.print(" CORR:");
            Serial.print(correction);

            Serial.print(" M1:");
            Serial.print(m1);

            Serial.print(" M2:");
            Serial.println(m2);
        }
    }
}