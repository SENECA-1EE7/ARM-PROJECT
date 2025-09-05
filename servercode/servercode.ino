#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP32Servo.h>
#include "EMGFilters.h"

// Servo Management Layer
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
  int openAngle;    // open (usually 0)
  int closeAngle;
  int lastAngle;    
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

// Wi-Fi / Web
const char *ssid = "ESP32-Server";
const char *password = "12345678";
AsyncWebServer server(80);


// HTML
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en"
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Robotic Hand Controller</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #0f0f23 0%, #1a1a2e 50%, #16213e 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 10px;
        }
        
        .container {
            background: white;
            border-radius: 20px;
            box-shadow: 0 25px 50px rgba(0, 0, 0, 0.25);
            padding: 15px;
            max-width: 95vw;
            width: 100%;
            text-align: center;
            max-height: 95vh;
            overflow: hidden;
        }
        
        h1 {
            color: #1f2937;
            font-size: 2.5rem;
            font-weight: 700;
            margin-bottom: 20px;
        }
        
        .hand-container {
            display: flex;
            justify-content: center;
            margin: 20px 0;
            background: linear-gradient(135deg, #f8fafc 0%, #e2e8f0 100%);
            border-radius: 25px;
            box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.1), 0 10px 30px rgba(30, 41, 59, 0.1), 0 1px 3px rgba(0, 0, 0, 0.1);
            padding: 20px 10px;
            position: relative;
        }
        
        .hand-container::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: radial-gradient(circle at 50% 30%, rgba(255, 255, 255, 0.2) 0%, transparent 70%);
            border-radius: 25px;
            pointer-events: none;
        }
        
        .hand-svg {
            max-width: 100%;
            height: auto;
            filter: drop-shadow(0 8px 25px rgba(30, 41, 59, 0.15));
            position: relative;
            z-index: 1;
        }
        
        .finger {
            cursor: pointer;
            transition: all 0.3s ease;
        }
        
        .finger:hover {
            opacity: 0.8;
        }
        
        .finger.active .finger-segment,
        .finger.active .finger-tip {
            fill: url(#activeGradient) !important;
        }
        /* Touch-friendly finger areas */
        
        .finger {
            touch-action: manipulation;
        }
        /* Mobile-specific improvements */
        
        @media (max-width: 768px) {
            .container {
                padding: 10px;
                max-height: 100vh;
            }
            
            h1 {
                font-size: 1.5rem;
                margin-bottom: 10px;
            }
            
            .hand-container {
                padding: 15px 10px;
                margin: 15px 0;
            }
            
            .hand-svg {
                width: 100% !important;
                height: 80vh !important;
                max-width: none !important;
                min-height: 80vh !important;
                max-height: none !important;
            }
            .finger {
                cursor: pointer;
            }
            .finger:active {
                opacity: 0.7;
            }
        }
        
        /* Extra mobile improvements for smaller screens */
        @media (max-width: 480px) {
            .container {
                padding: 8px;
                max-height: 100vh;
            }
            
            h1 {
                font-size: 1.3rem;
                margin-bottom: 8px;
            }
            
            .hand-container {
                padding: 10px 5px;
                margin: 10px 0;
            }
            
            .hand-svg {
                width: 100% !important;
                height: 85vh !important;
                min-height: 85vh !important;
                max-height: none !important;
            }
        }
        
        .finger-labels {
            font-size: 16px;
            font-weight: 700;
            fill: #64748b;
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
        }

        .status {
            margin-top: 15px;
            padding: 10px;
            background: #f1f5f9;
            border-radius: 10px;
            color: #475569;
            font-weight: 500;
        }
    </style>
</head>

<body>

    <div class="container">

        <h1>Robotic Hand Controller</h1>

        <div class="hand-container">
            <svg class="hand-svg" width="100%" height="auto" viewBox="0 0 800 900">
                <!-- Definitions for gradients and patterns -->
                <defs>
                    <!-- Palm base gradient -->
                    <linearGradient id="palmGradient" x1="0%" y1="0%" x2="100%" y2="100%">
                        <stop offset="0%" style="stop-color:#2d3748;stop-opacity:1" />
                        <stop offset="50%" style="stop-color:#4a5568;stop-opacity:1" />
                        <stop offset="100%" style="stop-color:#1a202c;stop-opacity:1" />
                    </linearGradient>
                    
                    <!-- Finger segment gradient -->
                    <linearGradient id="fingerGradient" x1="0%" y1="0%" x2="100%" y2="100%">
                        <stop offset="0%" style="stop-color:#475569;stop-opacity:1" />
                        <stop offset="30%" style="stop-color:#64748b;stop-opacity:1" />
                        <stop offset="70%" style="stop-color:#334155;stop-opacity:1" />
                        <stop offset="100%" style="stop-color:#1e293b;stop-opacity:1" />
                    </linearGradient>
                    
                    <!-- Joint gradient -->
                    <radialGradient id="jointGradient" cx="50%" cy="50%" r="50%">
                        <stop offset="0%" style="stop-color:#94a3b8;stop-opacity:1" />
                        <stop offset="50%" style="stop-color:#64748b;stop-opacity:1" />
                        <stop offset="100%" style="stop-color:#374151;stop-opacity:1" />
                    </radialGradient>
                    
                    <!-- Highlight gradient for metallic effect -->
                    <linearGradient id="highlightGradient" x1="0%" y1="0%" x2="0%" y2="100%">
                        <stop offset="0%" style="stop-color:#ffffff;stop-opacity:0.4" />
                        <stop offset="40%" style="stop-color:#ffffff;stop-opacity:0.1" />
                        <stop offset="100%" style="stop-color:#ffffff;stop-opacity:0" />
                    </linearGradient>
                    
                    <!-- Active finger gradient (green) -->
                    <linearGradient id="activeGradient" x1="0%" y1="0%" x2="100%" y2="100%">
                        <stop offset="0%" style="stop-color:#10b981;stop-opacity:1" />
                        <stop offset="30%" style="stop-color:#34d399;stop-opacity:1" />
                        <stop offset="70%" style="stop-color:#059669;stop-opacity:1" />
                        <stop offset="100%" style="stop-color:#047857;stop-opacity:1" />
                    </linearGradient>
                    
                    <!-- Shadow filter -->
                    <filter id="shadow" x="-20%" y="-20%" width="140%" height="140%">
                        <feDropShadow dx="2" dy="4" stdDeviation="3" flood-color="#1e293b" flood-opacity="0.3"/>
                    </filter>
                    
                    <!-- Metallic pattern -->
                    <pattern id="metalPattern" patternUnits="userSpaceOnUse" width="4" height="4">
                        <rect width="4" height="4" fill="#475569"/>
                        <circle cx="2" cy="2" r="0.5" fill="#64748b"/>
                    </pattern>
                </defs>
                
                <!-- Wrist/Base -->
                <rect x="280" y="720" width="240" height="120" rx="16" fill="url(#palmGradient)" stroke="#1f2937" stroke-width="3"/>
                <!-- Palm base -->
                <path d="M 240 690 Q 280 660 320 675 Q 360 667 400 675 Q 440 667 480 675 Q 520 660 560 690 L 560 720 L 240 720 Z" 
                      fill="url(#palmGradient)" stroke="#1f2937" stroke-width="3"/>
                
                <!-- Thumb (positioned on side) -->
                <g class="finger" data-finger="thumb" onclick="toggleFinger(this)">
                    <!-- Thumb metacarpal -->
                    <path d="M 240 720 Q 220 690 200 670 Q 180 650 160 640 L 180 610 Q 210 620 240 640 L 260 680 Q 250 700 240 720 Z" 
                          class="finger-segment" fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Thumb proximal -->
                    <rect class="finger-segment" x="144" y="525" width="58" height="112" rx="29" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Thumb distal -->
                    <rect class="finger-segment" x="152" y="465" width="54" height="80" rx="27" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Thumb tip -->
                    <ellipse class="finger-tip" cx="181" cy="457" rx="42" ry="53" 
                             fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Thumb joints -->
                    <circle cx="174" cy="622" r="13" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                    <circle cx="174" cy="533" r="11" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                </g>
                
                <!-- Index Finger -->
                <g class="finger" data-finger="index" onclick="toggleFinger(this)">
                    <!-- Index metacarpal -->
                    <rect class="finger-segment" x="261" y="645" width="64" height="80" rx="32" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Index proximal -->
                    <rect class="finger-segment" x="264" y="525" width="61" height="136" rx="30" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Index middle -->
                    <rect class="finger-segment" x="267" y="435" width="58" height="104" rx="29" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Index distal -->
                    <rect class="finger-segment" x="270" y="357" width="54" height="88" rx="27" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Index tip -->
                    <ellipse class="finger-tip" cx="298" cy="349" rx="37" ry="53" 
                             fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>                    
                    <!-- Index joints -->
                    <circle cx="291" cy="713" r="13" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                    <circle cx="294" cy="653" r="11" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                    <circle cx="296" cy="533" r="11" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                    <circle cx="298" cy="443" r="10" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                </g>
                
                <!-- Middle Finger -->
                <g class="finger" data-finger="middle" onclick="toggleFinger(this)">
                    <!-- Middle metacarpal -->
                    <rect class="finger-segment" x="349" y="645" width="64" height="80" rx="32" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Middle proximal -->
                    <rect class="finger-segment" x="352" y="509" width="61" height="152" rx="30" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Middle middle -->
                    <rect class="finger-segment" x="355" y="389" width="58" height="120" rx="29" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Middle distal -->
                    <rect class="finger-segment" x="358" y="285" width="54" height="104" rx="27" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Middle tip -->
                    <ellipse class="finger-tip" cx="386" cy="277" rx="37" ry="53" 
                             fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Middle joints -->
                    <circle cx="379" cy="713" r="13" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                    <circle cx="382" cy="653" r="11" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                    <circle cx="384" cy="517" r="11" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                    <circle cx="386" cy="397" r="10" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                </g>
                
                <!-- Ring Finger -->
                <g class="finger" data-finger="ring" onclick="toggleFinger(this)">
                    <!-- Ring metacarpal -->
                    <rect class="finger-segment" x="437" y="645" width="64" height="80" rx="32" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Ring proximal -->
                    <rect class="finger-segment" x="440" y="525" width="61" height="136" rx="30" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Ring middle -->
                    <rect class="finger-segment" x="443" y="413" width="58" height="112" rx="29" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Ring distal -->
                    <rect class="finger-segment" x="446" y="317" width="54" height="96" rx="27" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Ring tip -->
                    <ellipse class="finger-tip" cx="474" cy="309" rx="37" ry="53" 
                             fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Ring joints -->
                    <circle cx="467" cy="713" r="13" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                    <circle cx="470" cy="653" r="11" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                    <circle cx="472" cy="533" r="11" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                    <circle cx="474" cy="421" r="10" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                </g>
                
                <!-- Pinky -->
                <g class="finger" data-finger="pinky" onclick="toggleFinger(this)">
                    <!-- Pinky metacarpal -->
                    <rect class="finger-segment" x="520" y="661" width="61" height="72" rx="30" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Pinky proximal -->
                    <rect class="finger-segment" x="523" y="549" width="56" height="112" rx="27" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Pinky middle -->
                    <rect class="finger-segment" x="526" y="461" width="53" height="88" rx="26" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Pinky distal -->
                    <rect class="finger-segment" x="530" y="389" width="50" height="72" rx="24" 
                          fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Pinky tip -->
                    <ellipse class="finger-tip" cx="555" cy="381" rx="35" ry="48" 
                             fill="url(#fingerGradient)" stroke="#1f2937" stroke-width="2.4"/>
                    
                    <!-- Pinky joints -->
                    <circle cx="554" cy="725" r="11" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                    <circle cx="555" cy="669" r="10" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                    <circle cx="557" cy="557" r="10" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                    <circle cx="557" cy="469" r="8" fill="url(#jointGradient)" stroke="#1f2937" stroke-width="1.6"/>
                </g>
            </svg>
        </div>

        <div class="status" id="status">Click on any finger to toggle servo movement</div>

    </div>
<script>
    function toggleFinger(fingerElement) 
    {
        const fingerName = fingerElement.getAttribute('data-finger');
        let status;

        if (fingerElement.classList.contains('active')) 
        {
            fingerElement.classList.remove('active');
            status = "open";
            updateStatus(`${fingerName} finger open`);
        } 
        else 
        {
            fingerElement.classList.add('active');
            status = "close";
            updateStatus(`${fingerName} finger close`);
        }
        fetch(`/toggle?finger=${fingerName}&status=${status}`);
    }
    function updateStatus(text)
    {
        document.getElementById("status").textContent = text;
    }
</script>
</body>
</html>
)rawliteral";

void openfinger()
{
  Serial.println("Opening selected finger");
  Manager.openSelected();
}

void closefinger()
{
  Serial.println("Closing selected finger");
  Manager.closeSelected();
}

void moveservo()
{
  Manager.toggleSelected();
}

void controlFinger(const String &finger, const String &status)
{
  bool ok = false;
  if (status == "open") 
    ok = Manager.openByName(finger);
  else                   
    ok = Manager.closeByName(finger);
  if (!ok)
    Serial.println("Unknown finger: "+finger);
  Serial.println(finger + " finger " + status);
}

void setup()
{
  delay(3000);
  Serial.begin(115200);

  Manager.init();
  Manager.attachAll();

  WiFi.softAP(ssid, password);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  { 
    request->send_P(200, "text/html", index_html);
  });

  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request)
  {
    if (request->hasParam("finger") && request->hasParam("status"))
    {
      String finger = request->getParam("finger")->value();
      String status = request->getParam("status")->value();
      controlFinger(finger, status);
    }
    request->send(200, "text/plain", "OK");
  });
  server.begin();

}

void loop() 
{

}
