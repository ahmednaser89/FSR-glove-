# FSR Glove - Force Sensing Resistor Bluetooth Interface

This project reads a Force Sensing Resistor (FSR) using an ESP32, calibrates the sensor, and transmits the raw ADC value over Bluetooth Low Energy (BLE). It is designed for applications such as grip strength monitoring, gesture detection, or pressure sensing in gloves.

## Features

- **ADC reading** on GPIO14 (ADC2_CH3) with 12-bit resolution.
- **Calibration** – stores min/max values in NVS (non-volatile storage) to compute pressure percentage.
- **BLE GATT server** – provides a custom service with a characteristic that supports notifications.
- **Data transmission** – sends only the raw ADC value (0–4095) as a string to keep packets small and reliable.
- **Command handling** – listens for "CAL" to trigger a new calibration (useful for remote recalibration via MIT App Inventor).
- **Visual console output** – displays pressure level, voltage, and a bar graph for debugging.

## Hardware Requirements

- **ESP32 development board** (any with BLE support)
- **Force Sensing Resistor** (e.g., Interlink 402 or similar)
- **10 kΩ resistor** (pull‑down resistor to create a voltage divider)
- **Breadboard and jumper wires**

### Wiring Diagram

```
ESP32            FSR + Resistor
3.3V   -------- FSR one end
GPIO14 -------- FSR other end and 10kΩ resistor
GND    -------- other end of 10kΩ resistor
```

The FSR and the 10 kΩ resistor form a voltage divider. When pressure increases, the FSR resistance decreases, causing the voltage at GPIO14 to rise.

## Software Prerequisites

- **ESP-IDF** (Espressif IoT Development Framework) version 5.1 or newer (tested with v5.5.1).
- A terminal program (e.g., minicom, PuTTY, or built-in monitor) to view debug output.
- Optional: MIT App Inventor or any BLE client app (e.g., nRF Connect) to receive data.

## Building and Flashing

1. **Clone the repository** (if you have the source files, otherwise create the files as shown below).
2. **Set up the ESP-IDF environment** following the official [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/).
3. **Configure the project** (optional):
   ```bash
   idf.py menuconfig
   ```
   - Ensure the correct serial port is set under `Serial flasher config`.
   - Enable BLE if not already default.
4. **Build and flash**:
   ```bash
   idf.py build flash monitor
   ```

## Code Structure

| File | Description |
|------|-------------|
| `main.c` | Application entry point, creates tasks for BLE status and FSR monitoring. |
| `fsr_reader.h` / `fsr_reader.c` | Handles ADC initialisation, reading, calibration, and conversion to pressure percentage. |
| `ble_module.h` / `ble_module.c` | Implements BLE GATT server, advertising, and sending notifications. |
| `fsr_data_types.h` | Common data structure shared between modules. |
| `CMakeLists.txt` | Build configuration linking required components. |

## Calibration

Calibration is performed automatically if no saved calibration exists in NVS. The process:

1. **Min calibration** – do **not** touch the sensor for 5 seconds. The code averages 50 readings to set the "no pressure" value.
2. **Max calibration** – press the sensor as hard as possible for 5 seconds. The code records the maximum ADC value reached.

After calibration, the values are saved to NVS and used to compute the pressure percentage for future readings.

You can also trigger a recalibration remotely by sending the string `"CAL"` over BLE (write to the characteristic). The ESP32 will then restart the calibration process.

## BLE Service & Characteristics

The ESP32 advertises as **`ESP32 FSR Glove`** with a custom 128‑bit UUID service and characteristic.

- **Service UUID**: `12345678-1234-1234-1234-123456789012` (hex representation)
- **Characteristic UUID**: `12345678-1234-1234-1234-123456789013`
- **Properties**: Read, Write, Notify
- **Descriptor**: Client Characteristic Configuration Descriptor (CCCD) – required for enabling notifications.

### How to Receive Data

1. Scan for BLE devices and connect to `ESP32 FSR Glove`.
2. Find the characteristic with the above UUID.
3. Enable notifications by writing `0x01 0x00` (enable) to the CCCD descriptor (handle 0x2902). In nRF Connect, this is done by tapping the **download** icon next to the characteristic.
4. The ESP32 will then send the raw ADC value (as a string, e.g., `"3245"`) every 500 ms.

### Commands

- Write `"CAL"` (without quotes) to the characteristic to initiate a new calibration.

## Console Output

While running, the ESP32 prints a line to the serial console every 10 ms (but updates the BLE output only every 500 ms). Example:

```
ADC: 3095 | 2631 mV | HARD PRESS!  |  75% [###############.....]
```

- `ADC`: raw value (0–4095)
- `mV`: calculated voltage (assuming 3.3 V reference)
- `HARD PRESS!`: descriptive pressure level
- `75%`: pressure percentage based on calibration
- `[###############.....]`: visual bar (each # = 5% pressure)

## Customising the Project

- **Change GPIO** – edit `FSR_ADC_CHANNEL` and `FSR_ADC_UNIT` in `fsr_reader.h`. Use `ADC_CHANNEL_X` for GPIO pins.
- **Adjust notification rate** – modify the delay in `fsr_monitor_task` (currently 500 ms).
- **Change BLE device name** – edit `device_name` in `ble_module.c`.
- **Modify UUIDs** – change the service and characteristic UUIDs to match your MIT App Inventor project.

## Troubleshooting

### "attribute value too long, to be truncated to 20"
- The BLE MTU (Maximum Transmission Unit) default is 23 bytes (20 for data). Keep notifications under 20 bytes.
- This code sends only the raw value (max 4 digits), so it fits.

### Disconnections (reason 0x13)
- This often means the client (mobile app) terminated the connection. Ensure your MIT App Inventor app does not call `Disconnect` inadvertently.
- If using nRF Connect and it disconnects, try increasing the supervision timeout by adding connection parameter updates (see `ESP_GATTS_CONNECT_EVT` in `ble_module.c`).

### Stack overflow errors
- Increase the stack size of the tasks (e.g., `8192` for `fsr_monitor_task`) in `app_main`.

### No BLE advertising
- Check that BLE is properly initialised. The logs should show "✅ Advertising started".

## Using with MIT App Inventor

1. Add the **BluetoothLE** extension or use the built‑in BLE components (MIT App Inventor supports BLE via extensions).
2. Scan and connect to `ESP32 FSR Glove`.
3. Enable notifications on the characteristic.
4. In the `BytesReceived` event, parse the incoming string (which is the raw ADC value).
5. Optionally, send `"CAL"` to the characteristic to recalibrate.

Example MIT App Inventor logic:
- When button "Recalibrate" is clicked, call `BluetoothLE.Write` with text `"CAL"`.
- When bytes received, set a label to the value.

## License

This project is open‑source under the MIT License. Feel free to use and modify for your own applications.

## Acknowledgments

- ESP-IDF framework by Espressif Systems.
- FSR sensing concept from various maker projects.

