# ARM Project – EMG-Controlled Robotic Hand

## 1. Project Structure and Technologies

This project demonstrates a robotic arm that can be controlled in two modes:

* **Sensor-based control:** Using EMG signals to open and close all fingers.
* **Web-based control:** Using a browser interface to move each finger individually in real time.

The repository is organized as follows:

```
├── servo-code/        # Microcontroller code for EMG sensor input and servo control
├── servercode/          # Microcontroller code for hosting the website and finger control
├── libs/              # Libraries used for sensors and hardware
├── resources/         # Datasheets and documentation for sensors
└── README.md          # Project documentation
```

**Technologies used:**

* Microcontroller (Arduino/ESP32)
* EMG signal processing
* Servo motor actuation
* Embedded web server (ESP32/Arduino IDE)

---

## 2. Sensors, Resources, and Libraries

### Sensors

* **EMG sensor** – captures electrical activity from muscles to trigger hand movements.

### Libraries

* **Servo.h** – control of servo motors.
* **WiFi.h / WebServer.h (ESP32)** – for hosting the website and communication.
* **Arduino Serial** – debugging and IP display.

### Resources

* [EMG Sensor Documentation](https://www.olimex.com/Products/Duino/Shields/SHIELD-EKG-EMG/resources/)
* [Servo Motor Guide](https://lastminuteengineers.com/servo-motor-arduino-tutorial/)

---

## 3. Website Code
The website interface is hosted directly by the microcontroller. It allows controlling each finger individually in real time.
### Running the Website Code
1. Open the code from the `web-code/` folder in the Arduino IDE.
2. Upload it to your ESP32 (or compatible board).
3. Open the Serial Monitor at **115200 baud**. The microcontroller will print an **IP address**.
   Example output:

   ```
   Opening selected finger
   Closing selected finger
   AP IP: 192.168.4.1
   ```
4. On your computer or phone, connect to the Wi-Fi network created by the ESP32 (SSID and password are defined in the code).
5. Open a browser and enter the printed IP address (for example `192.168.4.1`).
6. The web interface will load, allowing you to control the robotic hand directly.
