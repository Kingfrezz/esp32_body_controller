#include <WiFi.h>
#include <ArduinoJson.h>

// WiFi AP configuration
const char* AP_SSID = "Carputer_ECU";
const char* AP_PASSWORD = "12345678";
const IPAddress AP_IP(192, 168, 4, 1);
const IPAddress AP_GATEWAY(192, 168, 4, 1);
const IPAddress AP_SUBNET(255, 255, 255, 0);

// TCP server for commands
WiFiServer server(5000);

// GPIO pins for body controls
// ULN2003A #1 (working)
const int PIN_DOOR_LOCK = 25;    // Door lock actuator
const int PIN_WINDOW_UP = 26;     // Window up relay
const int PIN_WINDOW_DOWN = 27;   // Window down relay
const int PIN_EXTRA1 = 32;        // Extra relay 1
const int PIN_EXTRA2 = 33;        // Extra relay 2

// ULN2003A #2 (working)
const int PIN_FAN1 = 18;          // Fan 1 relay
const int PIN_FAN2 = 12;          // Fan 2 relay
const int PIN_HVAC = 2;           // HVAC on/off relay
const int PIN_AC = 4;             // AC compressor relay
const int PIN_REMOTE_START = 5;   // Remote start relay

// Joypad pins
const int PIN_JOY_Y    = 36;   // VRy (analog, full 4-direction joystick)
const int PIN_JOY_X    = 39;   // VRx (analog, full 4-direction joystick)
const int PIN_BTN_SELECT = 22;  // Tact switch for Enter/Select
const int PIN_BTN_EXIT = 23;   // Tact switch for EXIT

// Status variables
bool hvacEnabled = false;
int fanSpeed = 0;              // 0-5
bool acEnabled = false;
bool doorsLocked = false;
bool remoteStartActive = false;
int fanRelayState = 0;         // 0=off, 1=fan1 only, 2=both fans
bool extraState[2] = {false, false};

// Joypad state (analog joystick + tact switches)
int m_joystickDir = -1;          // -1=center, 0=up, 1=down, 2=left, 3=right
bool m_joystickActive = false;   // true when a direction has been sent and not yet released
bool lastBtnSelect = HIGH;
bool lastBtnExit = HIGH;
unsigned long lastJoyReport = 0;
const unsigned long JOY_DEBOUNCE_MS = 250;

// Analog joystick thresholds (12-bit ADC: 0-4095)
const int JOY_THRESHOLD_MIN = 1200;
const int JOY_THRESHOLD_MAX = 2800;

// Client connection
WiFiClient client;
unsigned long lastClientCheck = 0;

void applyFanRelayState(int relayState) {
    fanRelayState = constrain(relayState, 0, 2);

    switch (fanRelayState) {
        case 0:
            digitalWrite(PIN_FAN1, LOW);
            digitalWrite(PIN_FAN2, LOW);
            break;
        case 1:
            digitalWrite(PIN_FAN1, HIGH);
            digitalWrite(PIN_FAN2, LOW);
            break;
        case 2:
            digitalWrite(PIN_FAN1, HIGH);
            digitalWrite(PIN_FAN2, HIGH);
            break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("Body Controller Starting...");

    // ULN2003A #1
    pinMode(PIN_DOOR_LOCK, OUTPUT);
    pinMode(PIN_WINDOW_UP, OUTPUT);
    pinMode(PIN_WINDOW_DOWN, OUTPUT);
    pinMode(PIN_EXTRA1, OUTPUT);
    pinMode(PIN_EXTRA2, OUTPUT);

    digitalWrite(PIN_DOOR_LOCK, LOW);
    digitalWrite(PIN_WINDOW_UP, LOW);
    digitalWrite(PIN_WINDOW_DOWN, LOW);
    digitalWrite(PIN_EXTRA1, LOW);
    digitalWrite(PIN_EXTRA2, LOW);

    // ULN2003A #2
    pinMode(PIN_FAN1, OUTPUT);
    pinMode(PIN_FAN2, OUTPUT);
    pinMode(PIN_HVAC, OUTPUT);
    pinMode(PIN_AC, OUTPUT);
    pinMode(PIN_REMOTE_START, OUTPUT);

    digitalWrite(PIN_FAN1, LOW);
    digitalWrite(PIN_FAN2, LOW);
    digitalWrite(PIN_HVAC, LOW);
    digitalWrite(PIN_AC, LOW);
    digitalWrite(PIN_REMOTE_START, LOW);

    // Joypad: analog joystick X/Y (GPIO36/39 are input-only, no pull)
    pinMode(PIN_JOY_Y, INPUT);
    pinMode(PIN_JOY_X, INPUT);
    pinMode(PIN_BTN_SELECT, INPUT_PULLUP);
    pinMode(PIN_BTN_EXIT, INPUT_PULLUP);

    // Start WiFi AP
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    delay(100);
    IPAddress myIP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(myIP);

    // Start TCP server
    server.begin();
    Serial.println("TCP server started on port 5000");

    // ADC calibration: print center readings
    Serial.print("ADC cal: X=");
    Serial.print(analogRead(PIN_JOY_X));
    Serial.print(" Y=");
    Serial.print(analogRead(PIN_JOY_Y));
    Serial.print(" (thresholds: min=");
    Serial.print(JOY_THRESHOLD_MIN);
    Serial.print(" max=");
    Serial.print(JOY_THRESHOLD_MAX);
    Serial.println(")");
}

void sendStatus() {
    if (!client.connected()) return;

    client.print("H:");
    client.print(hvacEnabled ? "1" : "0");
    client.print(" S:");
    client.print(fanSpeed);
    client.print(" A:");
    client.print(acEnabled ? "1" : "0");
    client.print(" L:");
    client.print(doorsLocked ? "1" : "0");
    client.print(" R:");
    client.print(remoteStartActive ? "1" : "0");
    client.print(" F:");
    client.print(fanRelayState);
    client.print(" P:");
    for (int i = 0; i < 2; i++) {
        if (i > 0) client.print(",");
        client.print(extraState[i] ? "1" : "0");
    }
    client.println();
}

void handleCommand(const String& cmd) {
    if (cmd.length() < 2) return;

    char action = cmd[0];
    int value = cmd.substring(1).toInt();

    switch (action) {
        case 'H':
            hvacEnabled = (value == 1);
            digitalWrite(PIN_HVAC, hvacEnabled ? HIGH : LOW);
            Serial.print("HVAC: "); Serial.println(hvacEnabled ? "ON" : "OFF");
            break;

        case 'S':
            fanSpeed = constrain(value, 0, 5);
            applyFanRelayState(fanSpeed == 0 ? 0 : (fanSpeed == 1 ? 1 : 2));
            Serial.print("Fan speed: "); Serial.println(fanSpeed);
            break;

        case 'A':
            acEnabled = (value == 1);
            digitalWrite(PIN_AC, acEnabled ? HIGH : LOW);
            Serial.print("AC: "); Serial.println(acEnabled ? "ON" : "OFF");
            break;

        case 'L':
            doorsLocked = (value == 1);
            digitalWrite(PIN_DOOR_LOCK, doorsLocked ? HIGH : LOW);
            Serial.print("Doors: "); Serial.println(doorsLocked ? "LOCKED" : "UNLOCKED");
            break;

        case 'W':
            if (value == 1) {
                digitalWrite(PIN_WINDOW_UP, HIGH);
                delay(500);
                digitalWrite(PIN_WINDOW_UP, LOW);
                Serial.println("Windows: UP");
            } else {
                digitalWrite(PIN_WINDOW_DOWN, HIGH);
                delay(500);
                digitalWrite(PIN_WINDOW_DOWN, LOW);
                Serial.println("Windows: DOWN");
            }
            break;

        case 'R':
            remoteStartActive = (value == 1);
            digitalWrite(PIN_REMOTE_START, remoteStartActive ? HIGH : LOW);
            Serial.print("Remote start: "); Serial.println(remoteStartActive ? "ACTIVE" : "OFF");
            if (remoteStartActive) {
                client.println("INFO:Remote start engaged");
            }
            break;

        case 'F':
            applyFanRelayState(value);
            Serial.print("Fan relay: "); Serial.println(fanRelayState);
            break;

        case 'P': {
            int idx = value / 10;
            int st  = value % 10;
            if (idx >= 1 && idx <= 2) {
                extraState[idx - 1] = (st == 1);
                int pins[] = {PIN_EXTRA1, PIN_EXTRA2};
                digitalWrite(pins[idx - 1], extraState[idx - 1] ? HIGH : LOW);
                Serial.print("Extra"); Serial.print(idx); Serial.print(": ");
                Serial.println(extraState[idx - 1] ? "ON" : "OFF");
            }
            break;
        }

        case 'C': {
            digitalWrite(PIN_EXTRA2, HIGH);
            delay(200);
            digitalWrite(PIN_EXTRA2, LOW);
            break;
        }

        case '?':
            sendStatus();
            return;
    }

    client.println("OK");
    sendStatus();
}

void checkJoypad() {
    if (!client || !client.connected()) return;

    unsigned long now = millis();
    if (now - lastJoyReport < JOY_DEBOUNCE_MS) return;

    // Analog joystick: read X/Y (12-bit ADC 0-4095)
    // Only sends command once per press (IDLE→PRESSED transition).
    // Must return to center before the same direction can fire again.
    int joyY = analogRead(PIN_JOY_Y);
    int joyX = analogRead(PIN_JOY_X);

    // Determine current direction
    int dir = -1; // center
    if (joyY > JOY_THRESHOLD_MAX) dir = 0;       // UP
    else if (joyY < JOY_THRESHOLD_MIN) dir = 1;   // DOWN
    else if (joyX < JOY_THRESHOLD_MIN) dir = 3;   // LEFT→RIGHT (inverted)
    else if (joyX > JOY_THRESHOLD_MAX) dir = 2;   // RIGHT→LEFT (inverted)

    if (dir == -1) {
        // Center → reset state
        if (m_joystickActive) {
            m_joystickActive = false;
            m_joystickDir = -1;
            lastJoyReport = now;
        }
    } else if (!m_joystickActive) {
        // IDLE → PRESSED transition: send once
        m_joystickActive = true;
        m_joystickDir = dir;
        lastJoyReport = now;
        const char* cmds[] = {"J:U", "J:D", "J:L", "J:R"};
        client.println(cmds[dir]); Serial.println(cmds[dir]); return;
    }

    // SELECT tact switch → Enter/Select
    bool s = digitalRead(PIN_BTN_SELECT);
    if (s == LOW && lastBtnSelect == HIGH) {
        lastBtnSelect = LOW; lastJoyReport = now;
        client.println("J:S");
        Serial.println("SELECT → J:S");
        return;
    }
    if (s == HIGH && lastBtnSelect == LOW) { lastBtnSelect = HIGH; lastJoyReport = now; }

    // EXIT tact switch
    bool e = digitalRead(PIN_BTN_EXIT);
    if (e == LOW && lastBtnExit == HIGH) {
        lastBtnExit = LOW; lastJoyReport = now;
        client.println("J:E"); Serial.println("J:E"); return;
    }
    if (e == HIGH && lastBtnExit == LOW) { lastBtnExit = HIGH; lastJoyReport = now; }
}

void loop() {
    if (!client || !client.connected()) {
        client = server.available();
        if (client) {
            Serial.println("New client connected");
        }
    }

    if (client && client.connected() && client.available()) {
        String cmd = client.readStringUntil('\n');
        cmd.trim();
        if (cmd.length() > 0) {
            Serial.print("Received: "); Serial.println(cmd);
            handleCommand(cmd);
        }
    }

    checkJoypad();
    delay(10);
}
