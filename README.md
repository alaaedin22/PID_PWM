# PID_PWM
Here are the complete wiring diagrams mapped exactly to the pin definitions in [CODE1.C](file:///Users/morsistoredz/my_first_backend/LAB1-OS/PID-PWM/CODE1.C).

---

### 1. Power Distribution & Buck Converter
> ⚠️ **Important:** Adjust the buck converter potentiometer to exactly **5.0 V** using a multimeter *before* connecting it to the ESP32.

```
 [ 7.4V - 8.4V 2S Battery ]
      (+)            (-)
       │              │
 [Slide Switch]       │
       │              │
       ├──────────────┼─────────────────────────► TB6612: VM (motor power)
       │              │
       ▼              ▼
  [ Buck Converter (set to 5V) ]
     IN+            IN-
     OUT+           OUT-
       │              │
       ├──────────────┼─────────────────────────► ESP32: VIN (5V pin)
       │              │
       ├──────────────┼─────────────────────────► Hall Sensor: VCC (needs 5V)
       │              │
       │              └─────────────────────────► COMMON GROUND RAIL (GND)
```

---

### 2. Motor Driver (TB6612FNG)

```
 ESP32                  TB6612 Driver                Motors
 ─────                  ─────────────                ──────
 GPIO 18 ─────────────► PWMA  (Left Speed)
 GPIO 16 ─────────────► AIN1  (Left Dir 1)
 GPIO 17 ─────────────► AIN2  (Left Dir 2)
                        AO1 ───────────────────────► LEFT Motor (+)
                        AO2 ───────────────────────► LEFT Motor (-)

 GPIO 19 ─────────────► PWMB  (Right Speed)
 GPIO 21 ─────────────► BIN1  (Right Dir 1)
 GPIO 22 ─────────────► BIN2  (Right Dir 2)
                        BO1 ───────────────────────► RIGHT Motor (+)
                        BO2 ───────────────────────► RIGHT Motor (-)

 Battery (+) via switch VM    (Motor power 7.4V)
 3.3V    ─────────────► VCC   (Logic power)
 3.3V    ─────────────► STBY  (Must be HIGH or motors are disabled)
 GND     ─────────────► GND   (Connect both driver GNDs to Common GND)
```

---

### 3. QTR-8RC Line Sensor Array (8 Sensors)

```
 ESP32                  QTR-8RC Sensor
 ─────                  ──────────────
 GPIO 32 ─────────────► Sensor 1  (Far Left)
 GPIO 33 ─────────────► Sensor 2
 GPIO 25 ─────────────► Sensor 3
 GPIO 26 ─────────────► Sensor 4
 GPIO 27 ─────────────► Sensor 5
 GPIO 14 ─────────────► Sensor 6
 GPIO 13 ─────────────► Sensor 7
 GPIO 23 ─────────────► Sensor 8  (Far Right)

 3.3V    ─────────────► VCC       (Use 3.3V logic to protect ESP32 inputs)
 GND     ─────────────► GND
                        LEDON / CTRL ──► Leave unconnected (IR LEDs stay ON)
```

---

### 4. Push Button & Status LEDs

```
 ESP32                  Resistor            LED / Button
 ─────                  ────────            ────────────
 GPIO 5  ─────────────────────────────────► Push Button Leg 1
                                            Push Button Leg 2 ──► GND
                                            (Uses internal INPUT_PULLUP)

 GPIO 4  ─────────────► 220 Ω ────────────► YELLOW LED (+) [Anode]
 GPIO 2  ─────────────► 220 Ω ────────────► GREEN  LED (+) [Anode]
 GPIO 15 ─────────────► 220 Ω ────────────► RED    LED (+) [Anode]

                                            All LED (−) [Cathodes] ──► GND
```

---

### 5. A3144 Hall Effect Sensor (Finish Line Detector)
> 💡 **Why this circuit:** The A3144 chip requires **5V** to operate, but ESP32 GPIOs cannot take 5V. Connecting the **10kΩ pull-up resistor to 3.3V** safely keeps the signal at 3.3V logic.

```
 Buck 5V ─────────────► Pin 1 (VCC)
 GND     ─────────────► Pin 2 (GND)
 GPIO 34 ◄────────┬─── Pin 3 (OUT)
                  │
                [10k Ω]
                  │
 3.3V   ──────────┘
```

---

### Summary Checklist of All ESP32 Pins Used

| Pin | Connected To | Function in Code |
| :--- | :--- | :--- |
| **GPIO 2** | Green LED (via 220Ω) | Ready / Racing indicator |
| **GPIO 4** | Yellow LED (via 220Ω) | Calibration mode indicator |
| **GPIO 5** | Push Button to GND | Calibration / Start arming trigger |
| **GPIO 13** | QTR Sensor 7 | Reflectance reading |
| **GPIO 14** | QTR Sensor 6 | Reflectance reading |
| **GPIO 15** | Red LED (via 220Ω) | Off-line alert / Finish line stop |
| **GPIO 16** | TB6612 AIN1 | Left Motor direction control |
| **GPIO 17** | TB6612 AIN2 | Left Motor direction control |
| **GPIO 18** | TB6612 PWMA | Left Motor 20 kHz PWM speed |
| **GPIO 19** | TB6612 PWMB | Right Motor 20 kHz PWM speed |
| **GPIO 21** | TB6612 BIN1 | Right Motor direction control |
| **GPIO 22** | TB6612 BIN2 | Right Motor direction control |
| **GPIO 23** | QTR Sensor 8 | Far Right reflectance reading |
| **GPIO 25** | QTR Sensor 3 | Reflectance reading |
| **GPIO 26** | QTR Sensor 4 | Center-left reflectance reading |
| **GPIO 27** | QTR Sensor 5 | Center-right reflectance reading |
| **GPIO 32** | QTR Sensor 1 | Far Left reflectance reading |
| **GPIO 33** | QTR Sensor 2 | Reflectance reading |
| **GPIO 34** | A3144 OUT (pull-up to 3.3V) | Finish line magnet detector |
