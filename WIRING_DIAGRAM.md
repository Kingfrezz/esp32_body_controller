# ESP32 Body Controller Wiring Diagram

## Overview
This document details the wiring connections between the ESP32 body controller and the vehicle's electrical system via ULN2003A Darlington transistor arrays.

## Pin Assignments & Color Coding

### ULN2003A Driver #1 (Control Pins: GPIO 25-33)
| ESP32 GPIO | Pin Name | Function | Wire Color | Active State | Notes |
|------------|----------|----------|------------|--------------|-------|
| GPIO 25 | PIN_DOOR_LOCK | Door Lock Actuator | **Purple** | LOW = UNLOCKED, HIGH = LOCKED | First wire mentioned - Purple for unlock |
| GPIO 26 | PIN_WINDOW_UP | Window Up Relay | Green | HIGH = Activate | Pulse to raise windows |
| GPIO 27 | PIN_WINDOW_DOWN | Window Down Relay | Yellow | HIGH = Activate | Pulse to lower windows |
| GPIO 32 | PIN_EXTRA1 | Extra Relay 1 | Orange | HIGH = Activate | Interior Light Control |
| GPIO 33 | PIN_EXTRA2 | Extra Relay 2 | Red | HIGH = Activate | Door Chime/Buzzer |

### ULN2003A Driver #2 (Control Pins: GPIO 2,4,5,12,18)
| ESP32 GPIO | Pin Name | Function | Wire Color | Active State | Notes |
|------------|----------|----------|------------|--------------|-------|
| GPIO 2 | PIN_HVAC | HVAC On/Off Relay | Blue | HIGH = ON | Heater/Blower control |
| GPIO 4 | PIN_AC | AC Compressor Relay | Cyan | HIGH = ON | Air conditioning clutch |
| GPIO 5 | PIN_REMOTE_START | Remote Start Relay | White | HIGH = Active | Engine start simulation |
| GPIO 12 | PIN_FAN2 | Fan 2 Relay | Brown | HIGH = ON | Secondary fan speed |
| GPIO 18 | PIN_FAN1 | Fan 1 Relay | Gray | HIGH = ON | Primary fan speed |

## Wiring Details

### Power Connections
- **ESP32 GND** → ULN2003A COM pins (common ground connection)
- **Vehicle Chassis Ground** → ESP32 GND (for reference)
- **Vehicle 12V Battery** → Relay coil power supplies (through fuses)
- **ESP32 3.3V** → ULN2003A input logic (if level shifting needed)

### Relay Circuit Connections
Each relay follows this pattern:
```
[Vehicle 12V+] → [Relay Coil] → [ULN2003A Output Pin] → [ESP32 GPIO]
                                                      ↓
                                                [ULN2003A COM] → [Ground]
```

### Door Lock System
- **Purple Wire (GPIO 25)** connects to door lock actuator coil
- **LOW Signal (0V)**: Actuator de-energized → Doors **UNLOCKED**
- **HIGH Signal (3.3V)**: Actuator energized → Doors **LOCKED**
- *Note: The actuator is likely a dual-coil or reversible type where polarity determines lock/unlock, or it's a simple latch where energize=lock, de-energize=unlock*

### Window Control
- **Green Wire (GPIO 26)**: Pulse to window up relay (500ms activation)
- **Yellow Wire (GPIO 27)**: Pulse to window down relay (500ms activation)
- *Note: Interlock prevents both up/down relays from activating simultaneously*

### Fan Speed Control (via Relay Combinations)
| Fan Speed | Fan 1 Relay (Gray) | Fan 2 Relay (Brown) | Fan Relay State |
|-----------|-------------------|---------------------|-----------------|
| 0 (Off) | OFF | OFF | 0 |
| 1 (Low) | ON | OFF | 1 |
| 2 (Med-Low) | ON | ON | 2 |
| 3 (Med) | ON | ON | 2* |
| 4 (Med-High) | ON | ON | 2* |
| 5 (High) | ON | ON | 2* |

*Note: Actual fan speed control may use PWM or additional circuitry not shown in this basic relay setup.*

### Extra Functions
- **Orange Wire (GPIO 32)**: Interior Light Control
  - HIGH = Lights ON
  - LOW = Lights OFF
- **Red Wire (GPIO 33)**: Door Chime/Buzzer
  - HIGH = Chime Active
  - LOW = Chime Off
  - *Also used for "C" command (chime pulse)*

### Special Functions
- **White Wire (GPIO 5)**: Remote Start
  - HIGH = Remote start engaged (simulates key turn)
  - LOW = Normal operation
- **Cyan Wire (GPIO 4)**: AC Compressor Clutch
  - HIGH = AC Engaged
  - LOW = AC Disengaged
- **Blue Wire (GPIO 2)**: HVAC Blower
  - HIGH = Blower ON
  - LOW = Blower OFF

## Communication Protocol
- **TCP Server**: ESP32 listens on port 5000
- **Command Format**: Single letter + value (e.g., "H1", "S3", "L0")
- **Response**: "OK" followed by status string: "H:X S:Y A:Z L:W R:V F:U P:A,B"
- **Status Query**: Send "?" to get current status without changing state

## Safety Notes
1. All relay coils should have flyback diodes installed
2. Use appropriate gauge wire for 12V vehicle circuits
3. Fuse all power connections near the battery source
4. Ensure proper isolation between ESP32 logic (3.3V) and vehicle systems (12V)
5. Consider adding optoisolators for additional isolation if needed

## Troubleshooting
- If doors won't unlock: Check purple wire (GPIO 25) - should be LOW for unlock
- If no response from ESP32: Verify it's powered and connected to WiFi AP "Carputer_ECU"
- If relays don't activate: Check ULN2003A grounding and 12V supply to relay coils