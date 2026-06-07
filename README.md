# WRO 2026 Future Engineers - Autonomous Vehicle Project

## 1. Project Overview & Team
Welcome to the official repository of IZTECH for the WRO 2026 Future Engineers competition. Developed by Nihan  and Elif , this repository contains the source code, electromechanical schematics, and mechanical design philosophy of our autonomous vehicle. 

Our core philosophy is **"Reliability through Modular State-Machine Architecture and Hardware-Level Optimization."** We aimed to solve common autonomous navigation problems—such as I2C conflicts and thread-blocking delays—using advanced multiplexing and non-blocking time-tracking methods.


---

## 2. Mobility and Mechanical Design
Our vehicle departs from the basic differential-drive (tank) mechanism and utilizes an **Ackermann Steering geometry**. 

* **Chassis & Drive:** We built the chassis using Lego Technic components to maintain a lightweight profile (< 1.5 kg) while staying within the 300x200x300 mm dimension constraints. Power is delivered to the rear axle via a geared DC motor for reliable torque.
* **Steering:** Front-wheel steering is controlled by a micro Servo motor. This provides realistic vehicle kinematics.
* **Trade-offs & Iteration:** Because an Ackermann vehicle cannot rotate in place (zero-radius turn), we iterated our U-Turn logic. Instead of counter-rotating wheels, the vehicle locks the steering servo to maximum deflection and drives forward, executing a wide sweeping arc tracked precisely by our IMU.

---

## 3. Power and Sensor Architecture
Our power and sensor network is designed for redundancy and noise reduction.

* **Power Management:** The system is powered by a high-discharge LiPo battery. To prevent voltage drops during motor spikes, the Arduino and sensors are powered through a dedicated 5V step-down buck converter, separating the logic circuit from the high-current L298N motor driver circuit.
* **Sensor Selection & Placement:**
  * **2x TCS34725 Color Sensors:** Placed at the front-left and front-right for robust traffic sign detection (Red/Green). 
  * **1* HC-SR04 Ultrasonic Sensors:** One faces directly forward for wall tracking and corner detection. The second is mounted at exactly 90 degrees on the right door to scan for the 36cm parking space during the final lap.
  ---

## 4. Software Architecture & Obstacle Strategy
We eliminated all thread-blocking `delay()` functions during active navigation, replacing them with a `millis()` based time-tracking logic. This prevents the robot from going "blind" during turns.

### The State Machine
Our C++ code is divided into a strict **Finite State Machine (FSM)** with the following states:
1. `DURUM_NORMAL_SUR` (Normal Drive): Polling sensors. If a Red pillar is detected on the right, it steers Left. If a Green pillar is on the left, it steers Right.
2. `DURUM_VIRAJ_DON` (Cornering): Triggered when the front HC-SR04 reads < 25cm. The servo turns, and the MPU6050 tracks the Yaw angle until an 80-90 degree differential is reached. 
3. `DURUM_U_DONUSU` (U-Turn): Triggered if the 2nd lap ends with a Red pillar. The IMU tracks a 175-degree change while executing a hard-steer forward arc, then reverses the logical driving direction.
4. `DURUM_PARK_ARA` (Park Search): Activated on the 3rd lap. The side ultrasonic sensor looks for a contiguous space > 36cm. 
5. `DURUM_PARK_YAP` (Execute Park): An open-loop hardcoded maneuver (Reverse-Right, Reverse-Left, Stop) to parallel park the vehicle.

---

## 5. System Thinking and Engineering Decisions
During development, we documented several risks and formulated mitigation strategies:
* **Constraint:** MPU6050 Gyro Drift over time.
  * **Mitigation:** We implemented a dynamic reference update (`referansAcisi = anlikAci`) after every successful 90-degree turn. The robot recalibrates its "zero" continuously, making it immune to cumulative long-term drift.
* **Constraint:** I2C Address Collisions on Color Sensors.
  * **Mitigation:** Transitioned from software-based power toggling to a dedicated TCA9548A hardware multiplexer, reducing CPU load and improving loop frequency to ~50Hz.

---

## 6. Build and Upload Process
To replicate this vehicle:
1. Clone this repository.
2. Ensure you have the following libraries installed in your Arduino IDE: `Wire.h`, `Servo.h`, `Adafruit_TCS34725.h`, and `MPU6050_light.h`.
3. Assemble the chassis according to the defined Ackermann steering geometry. Connect the sensors strictly following the TCA9548A channel definitions in the code (Ch 0 for Left Color, Ch 1 for Right Color).
4. Run the code. The robot initiates a 3-second stationary IMU calibration sequence on startup—**do not touch the vehicle during this phase.**
