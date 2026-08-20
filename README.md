# ESP32-Controlled 5-Axis Compact Robot Arm

This project is a high-performance recreation and upgrade of the 3D-printable [Compact Robot Arm (Arduino) by Build Some Stuff](https://www.printables.com/model/818975-compact-robot-arm-arduino-3d-printed). While the original design utilized a passive analog mimic controller, this version leverages the **ESP32** microcontroller coupled with a **PCA9685 16-channel 12-bit PWM driver** to offer dual-mode operation: **tactile physical joystick control** and a **remote WiFi-based web dashboard** featuring a real-time responsive SVG forward-kinematics visualization.

---

## Table of Contents
1. [Key Features](#key-features)
2. [Project Architecture & Directory Structure](#project-architecture--directory-structure)
3. [Bill of Materials (BOM)](#bill-of-materials-bom)
4. [Circuit & Wiring Diagram](#circuit--wiring-diagram)
5. [Pin Mapping](#pin-mapping)
6. [Software Features & Logic](#software-features--logic)
7. [Web Interface & HTTP API](#web-interface--http-api)
8. [3D Printed Parts & Assembly Notes](#3d-printed-parts--assembly-notes)
9. [Getting Started & Installation](#getting-started--installation)
10. [Credits & Attribution](#credits--attribution)

---

## Key Features

- **Dual Control Modes**:
  - **Physical Analog Joysticks**: Tactile manipulation of joints with real-time speed adjustments, plus integrated buttons for quick-closing and opening the gripper.
  - **WiFi Web Dashboard**: Broadcasts an independent Access Point (AP). Hosts a modern, glassmorphism-themed dark-mode dashboard with virtual on-screen joysticks and slider states.
- **Real-Time Visual Representation**: The web interface calculates a 2D side-profile of the robot arm using basic forward kinematics (trigonometric representation of links) and renders it dynamically in a vector-based HTML5 SVG.
- **Smooth Servo Ease Motion**: Prevents violent jerks on startup and during the home command by stepping servo movements incrementally using a custom [`slowMove()`](file:///c:/Users/MAKERBRAINS/Downloads/Compact%20Robot%20Arm/Compact_Robot_Arm_Code/Compact_Robot_Arm_Code.ino#L108) routine.
- **Automatic Calibration**: Automatically samples ADC offsets on boot to establish center positions and deadzones, filtering out joystick noise and drift.
- **Clean Cable Management**: The structural parts are hollow, enabling clean routing of servo motor cables inside the arm segments to avoid twisting and external clutter.

---

## Project Architecture & Directory Structure

```
Compact Robot Arm/
├── BD.png                        # System Block Diagram
├── Circuit Connections.png       # Schematic and Wiring Diagram
├── CAD/                          # 18 STL files for 3D Printing
│   ├── Actuator Support A.stl
│   ├── Actuator Support B.stl
│   ├── Base Rotation Ring.stl
│   ├── Body1.stl
│   ├── Elbow Joint Shaft.stl
│   ├── Elbow Pivot Spacer.stl
│   ├── Forearm Housing Left.stl
│   ├── Forearm Housing Right.stl
│   ├── Gripper Blade A.stl
│   ├── Gripper Blade B.stl
│   ├── Inner Upper Arm.stl
│   ├── Motor Access Plate.stl
│   ├── Outer Upper Arm.stl
│   ├── Servo Mount Plate.stl
│   ├── Shoulder Actuator Case.stl
│   ├── Shoulder Cable Guard.stl
│   ├── Shoulder Mount Plate.stl
│   └── Shoulder Support Plate.stl
├── Compact_Robot_Arm_Code/
│   └── Compact_Robot_Arm_Code.ino # Main Arduino C++ sketch for ESP32
└── README.md                     # Project documentation (this file)
```

---

## Bill of Materials (BOM)

### Electronics
1. **Microcontroller**: ESP32 Development Board (30-pin or 38-pin version).
2. **Servo Controller**: Adafruit PCA9685 16-Channel 12-bit PWM I2C Driver.
3. **Servos**:
   - **4x MG996R (or MG995) metal gear high-torque servos** (for Base, Shoulder, Elbow, and Wrist rotation).
   - **1x MG90S (or SG90) micro servo** (for the Gripper mechanism).
4. **Controllers**: 2x Analog Joystick Modules (e.g., KY-023 dual-axis potentiometer joysticks with integrated push-buttons).
5. **Power Supply**: 5V DC power supply with at least 2A output. Servos drawing current simultaneously under load will cause an ESP32 brownout if underpowered.
---

## Circuit & Wiring Diagram

![Block Diagram](BD.png)

The logical flow of signals follows the system Block Diagram (`BD.png`) and the wiring layout (`Circuit Connections.png`).

![Circuit Diagram](Circuit%20Connections.png)

---

## Pin Mapping

The physical wiring between the ESP32 microcontroller, Joysticks, and the PCA9685 is configured as follows in the firmware:

### 1. ESP32 to PCA9685 (I2C Communication)
- **SDA** $\rightarrow$ **GPIO 21**
- **SCL** $\rightarrow$ **GPIO 22**
- **I2C Address**: `0x40` (Default)

### 2. PCA9685 Servo Channels
| Channel | Servo Label | Description |
| :---: | :--- | :--- |
| **0** | `M1_CH` | **Base Rotation Servo** (M1) |
| **1** | `M2_CH` | **Shoulder Servo** (M2) |
| **2** | `M3_CH` | **Elbow Servo** (M3) |
| **3** | `M4_CH` | **Wrist Servo** (M4) |
| **4** | `M5_CH` | **Gripper Fingers Servo** (M5) |

### 3. Joysticks to ESP32
| Joystick Module | Joystick Pin | ESP32 GPIO | Associated Action |
| :--- | :---: | :---: | :--- |
| **Joystick 1** | X-Axis | **GPIO 34** | Controls **Shoulder (M2)** |
| **Joystick 1** | Y-Axis | **GPIO 35** | Controls **Base (M1)** |
| **Joystick 1** | Button Switch (SW) | **GPIO 25** | **Opens Gripper** (M5 $\rightarrow$ 90°) |
| **Joystick 2** | X-Axis | **GPIO 32** | Controls **Elbow (M3)** |
| **Joystick 2** | Y-Axis | **GPIO 33** | Controls **Wrist (M4)** |
| **Joystick 2** | Button Switch (SW) | **GPIO 27** | **Closes Gripper** (M5 $\rightarrow$ 0°) |

---

## Software Features & Logic

The project software is loaded in [`Compact_Robot_Arm_Code.ino`](file:///c:/Users/MAKERBRAINS/Downloads/Compact%20Robot%20Arm/Compact_Robot_Arm_Code/Compact_Robot_Arm_Code.ino). Below are descriptions of key functional parts:

### 1. Joystick Calibration ([`calibrateJoysticks()`](file:///c:/Users/MAKERBRAINS/Downloads/Compact%20Robot%20Arm/Compact_Robot_Arm_Code/Compact_Robot_Arm_Code.ino#L156-L205))
On start-up, the ESP32 instructs the user to leave the joysticks untouched for 2 seconds. It takes 100 analog samples from the ADC channels, averages them, and saves the values as references (`J1X_CENTER`, `J1Y_CENTER`, etc.). This calibrates out unique hardware offset errors and centers.

### 2. Noise & Deadzone Filtering ([`joystick()`](file:///c:/Users/MAKERBRAINS/Downloads/Compact%20Robot%20Arm/Compact_Robot_Arm_Code/Compact_Robot_Arm_Code.ino#L212-L239))
Reads the joystick inputs and implements a deadzone filter (`#define DEADZONE 260`) to avoid micro-drifting when the stick is resting. The movement is normalized between `-1.0` and `1.0` using custom scales for asymmetrical potentiometer bounds.

### 3. Ease-in Easing Algorithm ([`slowMove()`](file:///c:/Users/MAKERBRAINS/Downloads/Compact%20Robot%20Arm/Compact_Robot_Arm_Code/Compact_Robot_Arm_Code.ino#L108-L132))
Avoids abrupt servo snap actions. The function divides a large motion request into tiny steps (`stepSize = 0.8f`) separated by micro delays (`msPerStep = 8`), moving the robot smoothly to the target position:
```cpp
void slowMove(uint8_t channel, float fromAngle, float toAngle, float stepSize = 0.8f, int msPerStep = 8)
```

---

## Web Interface & HTTP API

The ESP32 runs a local Web Server on port 80 and initializes an Access Point:
- **SSID**: `RobotArm-ESP32`
- **Password**: `12345678`
- **IP Address**: `192.168.4.1`

### UI Mechanics & Kinematics Engine
- **Canvas Joysticks**: Recreates absolute physical joystick layouts on the screen, bound to click, drag, and touch gestures.
- **Forward Kinematics (SVG Representation)**: In the web script, a 2D side-profile of the robot is drawn using joint lengths (`SEG = [80, 70, 30]`). When servo positions are requested, it updates the SVG elements:
  - Base translation ($M_1$) shifts the base laterally on the page, representing side-view perspective depth.
  - Shoulder ($M_2$), Elbow ($M_3$), Wrist ($M_4$), and Gripper ($M_5$) angles calculate trigonometric relative coordinate positions ($x, y$) for the joint pivots and finger tips, updating the lines and labels live.

### API Endpoints
The server handles the following endpoints:
- `GET /` : Serves the core HTML/CSS/JS page from flash (`MAIN_PAGE`).
- `GET /motor?m=<1-4>&a=<angle>` : Sets a specific joint's target angle (0 to 180 degrees).
- `GET /gripper?action=<open|close>` : Directly commands the gripper servo (90° for open, 0° for closed).
- `GET /home` : Initiates a slow, synchronized homing sequence for all joints.
- `GET /status` : Returns a JSON object containing the current coordinate states:
  ```json
  {"m1": 100.0, "m2": 0.0, "m3": 180.0, "m4": 90.0, "m5": 0.0}
  ```

---

## 3D Printed Parts & Assembly Notes

The custom STL files for the structural parts are stored under the [`CAD/`](file:///c:/Users/MAKERBRAINS/Downloads/Compact%20Robot%20Arm/CAD) directory:
- **Structural arms**: `Inner Upper Arm.stl`, `Outer Upper Arm.stl`, `Forearm Housing Left.stl`, `Forearm Housing Right.stl`
- **Support Plates**: `Shoulder Support Plate.stl`, `Shoulder Mount Plate.stl`, `Servo Mount Plate.stl`
- **Actuators**: `Shoulder Actuator Case.stl`
- **Gripper**: `Gripper Blade A.stl`, `Gripper Blade B.stl`

### Printing Recommendations
- **Material**: PLA 
- **Layer Height**: `0.2 mm`
- **Supports**: Enable support structures for overhang segments.

---

### Firmware Upload
1. Open the [Arduino IDE](https://www.arduino.cc/en/software).
2. Install the **ESP32 board support package** (`Tools > Board > Boards Manager`).
3. Install the **Adafruit PWM Servo Driver Library** from the Library Manager.
4. Open [`Compact_Robot_Arm_Code.ino`](file:///c:/Users/MAKERBRAINS/Downloads/Compact%20Robot%20Arm/Compact_Robot_Arm_Code/Compact_Robot_Arm_Code.ino).
5. Select your ESP32 board and the corresponding COM port.
6. Connect your ESP32 via USB and upload the sketch.

### Initial Boot & Control
1. **Calibrate**: Power on the circuit. Ensure the joysticks are resting centered. The onboard calibration will compute within 2 seconds.
2. **Homing**: The arm will automatically execute a slow home posture sequence to initialize position tracking safely.
3. **Local Control**: Move the physical joysticks to actuate the arm. Press Joystick 1's click switch to open the gripper, and Joystick 2's click switch to close it.
4. **WiFi Control**:
   - On a phone or laptop, scan for WiFi networks and connect to `RobotArm-ESP32` using password `12345678`.
   - Open a browser and navigate to `http://192.168.4.1`.
   - Control the arm using the interactive web dashboard.

---

## Credits & Attribution

- **Original Mechanical Design**: Kelton at [Build Some Stuff](https://www.printables.com/@buildsomestf_1924578).
- **Mechanical CAD/STL files**: Downloaded from the [Compact Robot Arm Printables Page](https://www.printables.com/model/818975-compact-robot-arm-arduino-3d-printed).
- **Firmware, Network Stack, & Web App Upgrade**: Recreated and modified to run on ESP32 + PCA9685 with a visual web server dashboard.
