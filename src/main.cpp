#include <Arduino.h>
#include <ESP8266WiFi.h>

// ---------------------------------------------------------------------------
// Pin configuration (Wemos D1 Mini)
// ---------------------------------------------------------------------------
// D1 (GPIO5) and D2 (GPIO4) have no boot-strapping role and support the
// internal pull-ups, which makes them the safest choice for the link lines.
#define TIP  D1
#define RING D2

// The D1 Mini has a single on-board LED on D4 (GPIO2). It is active LOW.
// If you wire up external LEDs, change these pins and LED_ACTIVE_LOW to match.
#define LED_RX_TMT_PIN LED_BUILTIN
#define LED_TX_TMT_PIN LED_BUILTIN
#define LED_RX_PIN     LED_BUILTIN
#define LED_TX_PIN     LED_BUILTIN
#define LED_ACTIVE_LOW 1

// "Continue on timeout" button. The D1 Mini has no BOOT/FLASH button, so wire
// a momentary push button between D3 (GPIO0) and GND. D3 already has a pull-up
// on the board. Don't hold the button while powering on / resetting, or the
// ESP8266 will enter flash mode.
#define RETRY_PIN D3

// ---------------------------------------------------------------------------
// Timeouts
// ---------------------------------------------------------------------------
#define TIMEOUT_RX_US 500
#define TIMEOUT_TX_US 500000

bool error_level;

static inline void ledOn(uint8_t pin)  { digitalWrite(pin, LED_ACTIVE_LOW ? LOW : HIGH); }
static inline void ledOff(uint8_t pin) { digitalWrite(pin, LED_ACTIVE_LOW ? HIGH : LOW); }

// Block until the retry button is pressed. Yields so the ESP8266 watchdog
// doesn't reset the board while we wait for the user.
static void waitForRetry(uint8_t led_pin) {
    ledOn(led_pin);
    while (digitalRead(RETRY_PIN)) {
        delay(1);
    }
    ledOff(led_pin);
}

// Busy-wait while cond() is true. Returns false if timeout_us elapses first.
template <typename Cond>
static inline __attribute__((always_inline)) bool waitWhile(Cond cond, uint32_t timeout_us) {
    uint32_t start = micros();
    while (cond()) {
        if ((uint32_t)(micros() - start) > timeout_us) {
            return false;
        }
    }
    return true;
}

// Busy-wait while cond() is true. On timeout, light the timeout LED and wait
// for the retry button before continuing to wait.
template <typename Cond>
static inline __attribute__((always_inline)) void waitWhileOrRetry(Cond cond, uint32_t timeout_us, uint8_t tmt_led) {
    while (!waitWhile(cond, timeout_us)) {
        waitForRetry(tmt_led);
    }
}

// Timing-critical routines live in IRAM to avoid flash cache-miss jitter.
uint8_t IRAM_ATTR getByte() {
    uint8_t data = 0;
    error_level = 0;
    ESP.wdtFeed();
    for (int i = 0; i < 8; i++) {
        // Wait for the calculator to pull one of the lines low.
        if (!waitWhile([] { return digitalRead(TIP) && digitalRead(RING); }, TIMEOUT_RX_US)) {
            error_level = 1;
            ledOff(LED_RX_PIN);
            return 0;
        }
        ledOn(LED_RX_PIN);
        if (digitalRead(RING) == 0) {
            // Ring low -> bit is 1. Acknowledge by pulling tip low.
            data |= 1 << i;
            pinMode(TIP, OUTPUT);
            digitalWrite(TIP, LOW);
            waitWhileOrRetry([] { return !digitalRead(RING); }, TIMEOUT_RX_US, LED_RX_TMT_PIN);
            digitalWrite(TIP, HIGH);
            pinMode(TIP, INPUT_PULLUP);
        } else {
            // Tip low -> bit is 0. Acknowledge by pulling ring low.
            pinMode(RING, OUTPUT);
            digitalWrite(RING, LOW);
            waitWhileOrRetry([] { return !digitalRead(TIP); }, TIMEOUT_RX_US, LED_RX_TMT_PIN);
            digitalWrite(RING, HIGH);
            pinMode(RING, INPUT_PULLUP);
        }
    }
    ledOff(LED_RX_PIN);
    return data;
}

void IRAM_ATTR sendByte(uint8_t data) {
    error_level = 0;
    ESP.wdtFeed();
    for (int i = 0; i < 8; i++) {
        // Wait for both lines to be released (idle high).
        if (!waitWhile([] { return !digitalRead(TIP) || !digitalRead(RING); }, TIMEOUT_TX_US)) {
            error_level = 1;
            ledOff(LED_TX_PIN);
            return;
        }
        ledOn(LED_TX_PIN);
        if ((data >> i) & 1) {
            // Send a 1: pull ring low, wait for tip ack, release, wait for tip release.
            pinMode(RING, OUTPUT);
            digitalWrite(RING, LOW);
            waitWhileOrRetry([] { return digitalRead(TIP); }, TIMEOUT_TX_US, LED_TX_TMT_PIN);
            pinMode(RING, INPUT_PULLUP);
            waitWhileOrRetry([] { return !digitalRead(TIP); }, TIMEOUT_TX_US, LED_TX_TMT_PIN);
        } else {
            // Send a 0: pull tip low, wait for ring ack, release, wait for ring release.
            pinMode(TIP, OUTPUT);
            digitalWrite(TIP, LOW);
            waitWhileOrRetry([] { return digitalRead(RING); }, TIMEOUT_TX_US, LED_TX_TMT_PIN);
            pinMode(TIP, INPUT_PULLUP);
            waitWhileOrRetry([] { return !digitalRead(RING); }, TIMEOUT_TX_US, LED_TX_TMT_PIN);
        }
    }
    ledOff(LED_TX_PIN);
}

void setup() {
    // The radio isn't needed; turning it off removes its interrupts, which
    // would otherwise add jitter to the bit-banged link timing.
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    delay(1);

    Serial.begin(9600);

    pinMode(TIP, INPUT_PULLUP);
    pinMode(RING, INPUT_PULLUP);
    pinMode(LED_RX_TMT_PIN, OUTPUT);
    pinMode(LED_TX_TMT_PIN, OUTPUT);
    pinMode(LED_RX_PIN, OUTPUT);
    pinMode(LED_TX_PIN, OUTPUT);
    ledOff(LED_RX_TMT_PIN);
    ledOff(LED_TX_TMT_PIN);
    ledOff(LED_RX_PIN);
    ledOff(LED_TX_PIN);
    pinMode(RETRY_PIN, INPUT_PULLUP);
}

void loop() {
    while (1) {
        uint8_t data = getByte();
        if (!error_level) {
            Serial.write(data);
        } else {
            break;
        }
    }

    while (Serial.available()) {
        sendByte(Serial.read());
    }
}
