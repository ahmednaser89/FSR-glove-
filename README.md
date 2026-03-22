# FSR-glove-
develop a rugged FSR-based glove with 8–10 discrete force-sensing zones
## FSR Glove Pressure Monitor

This project implements a real-time pressure sensing system using a   Force Sensitive Resistor (FSR)   and an   ESP32  . It features a calibrated 0-100% pressure scale, visual feedback via a terminal bar graph, and wireless data transmission over   Bluetooth Low Energy (BLE)  .



  Hardware Configuration

The system uses a voltage divider circuit to convert the FSR's changing resistance into a measurable voltage for the ESP32 ADC.

    Microcontroller:   ESP32 (utilizing ADC2_CH3 / GPIO14).
    Sensor:   Force Sensitive Resistor (FSR).
    Pull-down Resistor:   2.5kΩ (optimized for better sensitivity range).
    Wiring:    3.3V  →  FSR  →  GPIO14  →  2.5kΩ Resistor  →  GND .





  Core Features

# 1. Dynamic Calibration
To account for sensor hardware variances, the project includes a two-phase calibration sequence:
    Min Calibration (5s):   Establishes the "No Touch" baseline by averaging 50 readings.
    Max Calibration (5s):   Sets the "Hard Press" ceiling by capturing the peak value during a firm squeeze.
    Persistent Storage:   Calibrated values are saved to   NVS (Non-Volatile Storage)  , so the device remembers its range after a reboot.

# 2. Pressure Mapping
The raw ADC values (0-4095) are mapped to a linear percentage based on the stored calibration:
    0% - 9%:    NO TOUCH 
    10% - 29%:    LIGHT TOUCH 
    30% - 69%:    MEDIUM PRESS 
    70% - 100%:    HARD PRESS! 

# 3. BLE Connectivity
The device acts as a BLE Server named   "ESP32_FSR_Glove"  .
    Data Streaming:   Sends formatted strings (e.g.,  FSR:raw,voltage,level ) to connected clients.
    Remote Command:   Supports a "CAL" write command to trigger a re-calibration remotely.



  Software Architecture

     fsr_reader.c/h   : Handles ADC initialization, curve-fitting calibration, and NVS flash operations.
     ble_module.c/h   : Manages the Bluetooth stack, advertising, and GATT event handling.
     main.c   : The orchestrator that initializes the system, loads calibration data, and runs the high-priority monitoring task.



  How to Use

1.    Monitor:   Open your Serial Monitor (115200 baud) to see the live bar graph and voltage readings.
2.    Calibrate:     On the first boot, follow the prompts to calibrate the sensor.
      To reset calibration at any time, send the string   "CAL"   via a BLE scanner app (like nRF Connect).
3.    Connect:   Look for the device in your BLE settings to begin receiving pressure data for mobile or desktop applications.

  Would you like me to help you create a  CMakeLists.txt  file to ensure all these modules compile correctly in your ESP-IDF environment?  
