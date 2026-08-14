# Firmware (PauloPaperMobileSed_rj.ino) and Configuration (CFG_1.txt)

This page provides comprehensive guidance on reading, uploading, and configuring the Mobile Turbidity System firmware and configuration file.

---

## Firmware Overview

The **PauloPaperMobileSed_rj.ino** firmware is designed for the **Turbidibuoy** — a floating mobile pumped turbidity and water level sensor system. The firmware coordinates multiple sensors and manages the turbidity measurement process through a peristaltic pump mechanism.

### Key Features
- **Multi-sensor support**: Acoustic Level Sensor (ALS), temperature sensors, Real-Time Clock (RTC), ultrasonic water level sensor
- **Turbidity measurement**: Forward pump for water intake, wet/dry readings with median calculation, reverse pump for backflushing
- **LoRa communication**: Wireless telemetry transmission at configurable intervals
- **SD card logging**: Continuous data logging to microSD card
- **Power management**: Sleep mode control and battery monitoring
- **Watchdog timer**: System stability through automatic reset on failures

### Firmware Architecture

#### Setup Phase (`setup()`)
- Initializes all hardware (SD card, LoRa radio, RTC, sensors, ADC)
- Detects connected sensors (OneWire temperature sensors are auto-detected)
- Reads configuration from SD card (`CFG_1.txt`)
- Synchronizes time with RTC
- Prepares CSV header for logging

#### Main Loop (`loop()`)
1. **Sensor acquisition** (if poll cycle reached):
   - Temperature sensors
   - RTC internal temperature
   - Acoustic Level Sensor (ALS) with temperature-compensated calibration
   - Ultrasonic water level sensor
   
2. **Turbidity measurement** (if water level > minimum):
   - Power pump speed control
   - **Dry read**: Takes 10 turbidity readings with pump off (baseline)
   - **Forward pump phase**: Pumps water through sensor for configured duration
   - **Wet read**: Takes 10 turbidity readings with water flowing
   - **Backflush phase**: Reverses pump to clear water from sensor
   - Calculates median values for both dry and wet readings

3. **Data logging & transmission**:
   - Builds CSV data string with all sensor values
   - Writes to SD card (opens new file if size exceeds 1.8 GB)
   - Transmits via LoRa at configured interval

4. **Sleep phase**:
   - Powers down sensors to reduce consumption
   - Sleeps until next poll period
   - Wakes on rain gauge interrupt or scheduled alarm

### Critical Configuration Variables (in CFG_1.txt)

| Variable | Purpose | Default |
|----------|---------|---------|
| `tpumptme` | Forward pump duration (seconds) | 10 |
| `tbflshtm` | Reverse pump duration (seconds) | 10 |
| `dryReadTimes` | Number of turbidity samples for median | 10 |
| `wetReadTimes` | Number of turbidity samples when wet | 10 |
| `minh2o` | Minimum water level to allow wet read (mm) | 20 |
| `turbPeriod` | Delay between turbidity reads (ms) | 100 |

---

## How to Read/Upload Firmware

### Requirements
- **Arduino IDE** (version 1.8.0 or later) or **Visual Studio Code with Arduino extension**
- **SAMD21 board package** installed (Board: Arduino MZero / Arduino SAMD)
- **Required libraries**:
  - RTClib
  - DFRobot_ADS1115
  - OneWire
  - DallasTemperature
  - RTCZero
  - RadioHead (RH_RF95)
  - QuickMedianLib
  - SDFat or built-in SD library

### Installation Steps

1. **Prepare the IDE**:
   - Install Arduino IDE from [arduino.cc](https://www.arduino.cc/en/software)
   - Go to `Tools > Board > Boards Manager`
   - Search and install "Arduino SAMD Boards"
   - Select `Tools > Board > Arduino MZero`

2. **Install Required Libraries**:
   - Open `Sketch > Include Library > Manage Libraries`
   - Search for and install each library listed above
   - Alternatively, use the library manager or copy `.zip` files to the `libraries/` folder

3. **Download the Firmware**:
   - Clone or download `PauloPaperMobileSed_rj.ino` from this repository
   - Open the file in Arduino IDE

4. **Configure Build Settings**:
   ```
   Tools > Board:           Arduino MZero
   Tools > Port:            COM[X] (or /dev/ttyACM[X] on Linux/Mac)
   Tools > Programmer:      Atmel EDBG
   Tools > Upload Speed:    57600
   ```

5. **Upload to Device**:
   - Connect the MobileTurbidity hardware via USB
   - Click **Upload** (→ button) or press `Ctrl+U`
   - Wait for `"Verify successful"` message
   - Monitor serial output via `Tools > Serial Monitor` (115200 baud) for startup messages

### Reading the Firmware
- The firmware is well-commented and organized into sections
- Key sections:
  - Lines 1–175: Includes and pin definitions
  - Lines 176–364: `setup()` function
  - Lines 370–488: `loop()` function
  - Lines 495–1109: Helper functions

---

## Configuration File (CFG_1.txt)

The **CFG_1.txt** file controls system behavior without requiring code recompilation. It must be placed in the **root of the SD card**.

### File Format Rules
- **Format**: `ConfigVariable=Value;`
- Whitespace is ignored
- Comments after semicolon are ignored
- Lines without a config variable are ignored
- `DEFAULT` keyword skips a setting (uses firmware default)
- **Filename must match firmware version**: Firmware version 1 → `CFG_1.txt`

### Configuration Parameters

#### System Config
```
Project_ID        = CCK;        // 3-character project identifier
Site_ID           = CCK;        // 3-character site identifier
Node_ID           = SED;        // 4-character node identifier
LoRaPollOffset    = 13;         // Packet send offset (seconds into poll period)
PollPerLoRa       = 1;          // Cycles per LoRa transmission (1 = send every cycle)
LoRa_EN           = 1;          // LoRa enabled (0=off, 1=on)
SD_EN             = 1;          // SD logging enabled (0=off, 1=on)
```

#### Sensors
```
ALS_Count         = 1;          // Acoustic Level Sensors (0 or 1)
ALS_Period        = 1;          // ALS poll period (minutes)
ALS_cal_temps     = {4, 33};    // Temperature points for ALS calibration (ascending)
ALS_cal_slopes    = {0.511, 0.511};  // Slope values (one per temp point)
ALS_cal_offsets   = {-505, -505};    // Offset values (one per temp point)

Temp_Count        = 2;          // Temperature sensors (auto-detected, read-only)
Temp_Period       = 1;          // Temperature poll period (minutes)

USS_Count         = 0;          // Ultrasonic sensors (0 or 1)
USS_Period        = DEFAULT;    // Ultrasonic poll period (minutes)

RTC_Temp_Count    = 1;          // Use RTC temperature (0 or 1)
RTC_Temp_Period   = 1;          // RTC temperature poll period (minutes)
```

#### Turbidity Measurement
```
tpumptme          = 6;          // Forward pump duration (seconds)
tbflshtm          = 9;          // Reverse pump duration (seconds)
dryread           = 10;         // Dry turbidity readings for median
wetread           = 10;         // Wet turbidity readings for median
minh2o            = 20;         // Minimum water level (mm) to allow measurement
turbPd            = 100;        // Delay between turbidity reads (milliseconds)
```

### ALS Calibration (Temperature Compensation)

The Acoustic Level Sensor output varies with temperature. Calibration uses **linear interpolation** between calibration points.

**Example**:
```
ALS_cal_temps     = {4, 25, 40};
ALS_cal_slopes    = {0.500, 0.515, 0.520};
ALS_cal_offsets   = {-500, -510, -515};
```

At 15°C (between 4 and 25°C):
- Interpolation ratio = (15 - 4) / (25 - 4) ≈ 0.52
- Slope ≈ 0.52 × (0.515 - 0.500) + 0.500 = 0.508
- Offset ≈ 0.52 × (-510 - (-500)) + (-500) = -505.2

---

## How to Read/Edit/Upload Configuration

### Reading the Configuration

1. **Access the SD card**:
   - Remove the microSD card from the MobileTurbidity device
   - Insert into a computer's SD card reader

2. **Open CFG_1.txt**:
   - Use any text editor (Notepad, VS Code, Sublime, etc.)
   - The file is plain text; no special software required

3. **View current settings**:
   - Each line shows one parameter and its value
   - `DEFAULT` means the firmware's hardcoded value is used
   - Disabled lines start with `;` (semicolon)

### Editing the Configuration

1. **Locate the parameter** you want to change
   - Refer to the table above for descriptions

2. **Modify the value**:
   - **DO NOT** change the variable name (left side of `=`)
   - Change only the value between `=` and `;`
   - Ensure commas separate array elements (e.g., `{4,33}`)

3. **Common edits**:
   
   **Change pump timing** (for faster/slower water intake):
   ```
   tpumptme = 8;   // Increase for more water injection time
   ```

   **Adjust minimum water level**:
   ```
   minh2o = 30;    // Require 30mm depth before measuring
   ```

   **Enable/disable sensors**:
   ```
   ALS_Count = 1;  // Use ALS
   USS_Count = 0;  // Do not use ultrasonic
   ```

   **Change poll frequency**:
   ```
   ALS_Period = 5; // Read ALS every 5 minutes (was 1)
   ```

4. **Save the file**:
   - Use `File > Save` or `Ctrl+S` (same format, no conversion needed)

### Uploading (Installing) Configuration

1. **Reinsert SD card**:
   - Place the edited `CFG_1.txt` into the SD card root directory
   - Ensure the filename matches the firmware version (`CFG_1.txt` for firmware v001)

2. **Power on the device**:
   - Insert the SD card into the MobileTurbidity device
   - Power on; the system will automatically read `CFG_1.txt` during startup
   - Monitor LoRa/serial output to confirm configuration was applied

3. **Verify changes**:
   - The firmware transmits configuration values on startup (LoRa IGN messages)
   - Watch for messages like:
     ```
     "forward pump time tpumptme = 8 sec"
     "reverse pump time tbflshtm = 9 sec"
     "period between turbidity measures turbPeriod = 100ms"
     ```

4. **If config is not applied**:
   - Check that `CFG_1.txt` exists in the SD card root
   - Ensure filename matches firmware version number
   - Verify no typos in variable names (case-sensitive)
   - Check for corrupt SD card (reformat if needed)
   - Review `debug.txt` on the SD card for error messages

### Troubleshooting Configuration Issues

| Issue | Cause | Solution |
|-------|-------|----------|
| Config not applied | File not found | Ensure `CFG_1.txt` (exact name) in root of SD card |
| Config not applied | Wrong firmware version | Rename file to match firmware version (e.g., `CFG_2.txt` for v002) |
| Sensor not responding | `*_Count` set to 0 | Change `ALS_Count=1`, `USS_Count=1`, etc. |
| Pump runs too long/short | `tpumptme` incorrect | Adjust value (in seconds) |
| Water not sampled | `minh2o` set too high | Lower threshold or check water level sensor |
| Syntax error on startup | Malformed config line | Check for missing `;`, mismatched `{}`, or typos |

---

## CSV Data Logging Format

The firmware logs data to SD card in CSV format. Column order:
```
DATE_TIME, SITE_ID, NODE_ID, FW_VER, COUNT, UPTIME, FILE_SIZE, BATT_V, 
Temp_0, Temp_1, ..., RTCTemp, RawALS_0, EstLevel_0, ..., USS, 
TURB_DRY, TURB_WET, HOUSING_TEMP, LEAK_SENSOR
```

File naming: `{SiteID}{NodeID}{Letter}.csv` (e.g., `CCKSEDK.csv`)

---

## Related Resources

- [Firmware source code](PauloPaperMobileSed_rj.ino)
- [Default configuration](CFG_1.txt)
- [Arduino IDE Documentation](https://docs.arduino.cc/software/ide-v2)
- [SAMD21 Microcontroller Datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/DataSheets/SAM-D21-DA1-Family-Data-Sheet-DS40001882E.pdf)
