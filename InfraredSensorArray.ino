struct Sensor {
  int pin;
  int raw;       
  int realMin;   
  int realMax;   
  int current;   
  bool started;  // NEW: Keeps track of whether this sensor has started its sweep yet
};

const int NUM_SENSORS = 7;



// We initialize 'started' to false for all sensors
Sensor sensors[NUM_SENSORS] = {
  {14, 0, 0, 0, 0, false}, 
  {15, 0, 0, 0, 0, false},
  {16, 0, 0, 0, 0, false},
  {17, 0, 0, 0, 0, false},
  {18, 0, 0, 0, 0, false},
  {19, 0, 0, 0, 0, false},
  {20, 0, 0, 0, 0, false}
};

const unsigned long SWEEP_THRESHOLD = 5000; 
unsigned long currentTime = 0;
unsigned long lastSerialTime = 0;

bool isSweeping = false;
void setup() {
  Serial8.begin(115200);
  Serial.println(">>> START KINETIC SWEEP NOW <<<");
  currentTime = millis();
  
  while (millis() - currentTime < SWEEP_THRESHOLD) {
    kineticSweep();
    for (int i = 0; i < NUM_SENSORS; i++) {
      readSensorHardware(sensors[i]);      // Function 1: Updates s.raw
      compareAndTrackExtremes(sensors[i]); // Function 2: Evaluates and saves extremes
    }
  }
  Serial.println(">>> KINETIC SWEEP COMPLETE. BASELINES LOCKED. <<<");
}
   float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
     float result;
     result = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
     return result;
}
void loop() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    readSensorHardware(sensors[i]); 
    
    sensors[i].current = mapf(sensors[i].raw, sensors[i].realMin, sensors[i].realMax, 0, 100);
    sensors[i].current = constrain(sensors[i].current, 0, 100);
    
    Serial.print("S"); Serial.print(i); Serial.print(": ");
    Serial.print(sensors[i].current); Serial.print("%\t");
  }
  Serial.println();
  delay(100);
}

void readSensorHardware(Sensor &s) {
  s.raw = analogRead(s.pin);
}

// FUNCTION 2 (FIXED): No longer trapped by hardcoded 0 or 1023 limits
void compareAndTrackExtremes(Sensor &s) {
  // If this is the absolute first read of the sweep, lock it in as the starting baseline
  if (!s.started) {
    s.realMin = s.raw;  // e.g., If sensor is resting at 200, min becomes 200
    s.realMax = s.raw;  // max also becomes 200
    s.started = true;   // Mark that initialization is done
    return;             // Exit function early for this first run
  }
  
  // For all subsequent runs of the sweep, expand outward based on real movement
  if (s.raw < s.realMin) {
    s.realMin = s.raw;  // If it dips to 180, 180 becomes the new min
  }
  if (s.raw > s.realMax) {
    s.realMax = s.raw;  // If it climbs to 980, 980 becomes the new max
  }
}
void kineticSweep () {
  unsigned long elapsedTime = millis() - currentTime;
  

  if (elapsedTime <= 2000) {
    Serial8.write(20);
    Serial8.write(230);
  } else if (elapsedTime >=2000 && elapsedTime <= 4000) {
    Serial8.write(125);
    Serial8.write(190);
  } else {
    Serial8.write(0);
  }


}
