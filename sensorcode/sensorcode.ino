#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include "EMGFilters.h"

Servo thumb, indexFinger, middleFinger, ringFinger, pinkyFinger;

const int PIN_THUMB  = 18;
const int PIN_INDEX  = 19;
const int PIN_MIDDLE = 21;
const int PIN_RING   = 22;
const int PIN_PINKY  = 23;

struct FingerEntry 
{
  const char  *name;
  Servo *servo;
  int openAngle;    // open posture (usually 0)
  int closeAngle;   // closed posture (usually 180)
  int lastAngle;    // last commanded angle (for smooth sweeps)
};

enum FingerID { F_THUMB=0, F_INDEX, F_MIDDLE, F_RING, F_PINKY, F_COUNT };

class ServoManager 
{
  public:
  void init() 
  {
    table[F_THUMB]  = {"thumb",  &thumb,       0, 160, 0};
    table[F_INDEX]  = {"index",  &indexFinger, 0, 100, 0};
    table[F_MIDDLE] = {"middle", &middleFinger,0, 140, 0};
    table[F_RING]   = {"ring",   &ringFinger,  0, 110, 0};
    table[F_PINKY]  = {"pinky",  &pinkyFinger, 0, 180, 0};
  }

  void attachAll() 
  {
    thumb.attach(PIN_THUMB);
    indexFinger.attach(PIN_INDEX);
    middleFinger.attach(PIN_MIDDLE);
    ringFinger.attach(PIN_RING);
    pinkyFinger.attach(PIN_PINKY);

    for (int i = 0 ;i < F_COUNT; i++) 
    {
      table[i].servo->write(table[i].openAngle);
      table[i].lastAngle = table[i].openAngle;
    }
  }

  bool selectByName(const String &finger) 
  {
    int id = idFromName(finger);
    if (id < 0) 
      return false;
    selected = (FingerID)id;
    return true;
  }

  const char* selectedName() const { return table[selected].name; }

  void openSelected()  
  { 
    moveTo(*table[selected].servo, table[selected].openAngle, table[selected].lastAngle);
    table[selected].lastAngle = table[selected].openAngle;
  }
  void closeSelected() 
  {
    moveTo(*table[selected].servo, table[selected].closeAngle, table[selected].lastAngle);
    table[selected].lastAngle = table[selected].closeAngle;
  }

  void toggleSelected() 
  {
    bool isOpen = (table[selected].lastAngle <= table[selected].openAngle);
    if (isOpen) 
      closeSelected(); 
    else 
      openSelected();
  }

  void openAll()
  {
    for(int i = 0;i < F_COUNT; i++)
    {
      moveTo(*table[i].servo, table[i].openAngle, table[i].lastAngle);
      table[i].lastAngle = table[i].openAngle;
    }
  }

  void closeAll()
  {
    for(int i = 0;i < F_COUNT; i++)
    {
      moveTo(*table[i].servo, table[i].closeAngle, table[i].lastAngle);
      table[i].lastAngle = table[i].closeAngle;
    }
  }

  bool openByName(const String &finger)  
  { 
    int id = idFromName(finger);
    if(id < 0) 
      return false;
    moveTo(*table[id].servo, table[id].openAngle, table[id].lastAngle); table[id].lastAngle = table[id].openAngle; 
    return true; 
  }

  bool closeByName(const String &finger)
  {
    int id = idFromName(finger);
    if( id < 0)
      return false;
    moveTo(*table[id].servo, table[id].closeAngle, table[id].lastAngle);
    table[id].lastAngle = table[id].closeAngle;
    return true;
  }

 private:
  FingerEntry table[F_COUNT];
  FingerID selected = F_THUMB; // EMG controls this one

 void moveTo(Servo &servo, int target, int current) 
 {
    int fingerIndex = -1;
    for(int i = 0; i < F_COUNT; i++) 
    {
      if(table[i].servo == &servo) 
      {
        fingerIndex = i;
        break;
      }
    }
    if(fingerIndex >= 0) 
    {
      if (current == table[fingerIndex].closeAngle) 
         target = table[fingerIndex].openAngle;
      if (current == table[fingerIndex].openAngle) 
         target = table[fingerIndex].closeAngle;
    }
    servo.write(target);
    delay(30);
}
  static int idFromName(const String &finger) 
  {
    if (finger.equalsIgnoreCase("thumb"))
      return F_THUMB;
    if (finger.equalsIgnoreCase("index"))
      return F_INDEX;
    if (finger.equalsIgnoreCase("middle"))
      return F_MIDDLE;
    if (finger.equalsIgnoreCase("ring"))
      return F_RING;
    if (finger.equalsIgnoreCase("pinky"))
      return F_PINKY;
    return -1;
  }
};

ServoManager Manager;

int state = 0;
int SPin = 34;
EMGFilters Filter;
bool allHeld = false; 
unsigned long threshold = 0;
unsigned long lastToggleMillis = 0;
NOTCH_FREQUENCY EFR = NOTCH_FREQ_50HZ;
SAMPLE_FREQUENCY Rate = SAMPLE_FREQ_500HZ;
const unsigned long LOCKOUT_AFTER_TOGGLE = 1000;


int getEMGCount(int envelope)
{
  static long integralData = 0;
  static long integralDataEve = 0;
  static bool remainFlag = false;
  static unsigned long timeMillis = 0;
  static unsigned long timeBeginzero = 0;
  static long fistNum = 0;
  static int TimeStandard = 200;

  integralDataEve = integralData;
  integralData += envelope;

  if ((integralDataEve == integralData) && (integralDataEve != 0))
  {
    timeMillis = millis();
    if (remainFlag)
    {
      timeBeginzero = timeMillis;
      remainFlag = false;
      return 0;
    }
    if ((timeMillis - timeBeginzero) > TimeStandard)
    {
      integralDataEve = integralData = 0;
      return 1;
    }
    return 0;
  }
  else 
  {
    remainFlag = true;
    return 0;
  }
}


void setup()
{
  delay(3000);
  Serial.begin(115200);

  Manager.init();
  Manager.attachAll();

  Filter.init(Rate, EFR, true, true, true);
  Serial.println("Calibrating EMG sensor... Keep muscle relaxed for 3 seconds");
  delay(1000);
  
  long sum = 0;
  for(int i = 0; i < 100; i++) 
  {
    int data = analogRead(SPin);
    int filtered = Filter.update(data);
    int envelope = sq(filtered);
    sum += envelope;
    delay(30);
  }
  threshold = sum / 100; 
  threshold *= 1.5;
  
  Serial.print("Threshold set to: ");
  Serial.println(threshold);
  Serial.println("EMG sensor ready! Flex muscle to control hand.");
  delay(10);
}

void loop() 
{
  int data = analogRead(SPin);
  int dataAfterFilter = Filter.update(data);
  int envelope = sq(dataAfterFilter);
  
  if (envelope <= threshold)
    envelope = 0;
  if (threshold > 0)
  {
    if (getEMGCount(envelope))
    {
      unsigned long now = millis();
      if (now - lastToggleMillis > LOCKOUT_AFTER_TOGGLE) 
      {
        Serial.println("EMG: Opening all fingers");
        Manager.closeAll();
        delay(2000);
        Serial.println("EMG: Closing all fingers");
        Manager.openAll();
        lastToggleMillis = now;
      }
    }
  }
  delayMicroseconds(500);
}


// void loop() 
// {
//   int data = analogRead(SPin);
//   int dataAfterFilter = Filter.update(data);
//   int envelope = sq(dataAfterFilter);
  
//   if (envelope <= threshold)
//     envelope = 0;
//   if (threshold > 0)
//   {
//     if (getEMGCount(envelope))
//     {
//       unsigned long now = millis();
//       if (now - lastToggleMillis > LOCKOUT_AFTER_TOGGLE) 
//       {
//         if (muscleActive) 
//         {
//           Serial.println("EMG: Opening all fingers");
//           Manager.openAll();
//           muscleActive = false;
//         } 
//         else 
//         {
//           Serial.println("EMG: Closing all fingers");
//           Manager.closeAll();
//           muscleActive = true;
//         }
//         lastToggleMillis = now;
//       }
//     }
//   }
//   delayMicroseconds(500);
// }
