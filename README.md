# ESP8266-Ti-Link
ESP8266-Ti-Link is an open-source project that lets a Texas Instruments graphing calculator talk to a PC through a Wemos D1 Mini (ESP8266), which acts as a link cable.

This is a port of [ESP32-Ti-Link](https://github.com/tesa-klebeband/ESP32-Ti-Link) to the Wemos D1 Mini.

## Features
- Transfer data between TI graphing calculators and a PC
- Supports transfer of variables, programs, applications, the OS and more
- Detects communication errors

## Getting started
Clone this repository using:

`git clone https://github.com/willasaywhat/esp8266-ti-link.git`

### Adapter setup
1. Strip the insulation of a 2.5mm cable or solder wires to a 2.5mm plug
2. Connect the ground or shield of the cable to the D1 Mini's **G** (GND) pin
3. Connect the tip wire to **D1** (GPIO5), or choose your own by changing the TIP definition in main.cpp
4. Connect the ring wire to **D2** (GPIO4), or choose your own by changing the RING definition
5. (Optional) Wire a push button between **D3** (GPIO0) and **G** to use as the "continue on timeout" button (see [Timeouts](#timeouts))
6. Download and install the CH340 USB-serial driver (used by most D1 Mini boards)
7. Install PlatformIO and the PlatformIO extension in VS Code
8. Open the repository in VS Code
9. Connect the D1 Mini and upload the code

The ESP8266 runs at 3.3V. The link lines are open-drain and only ever pulled low by the D1 Mini, the same way the original ESP32 version works.

If you pick other pins, avoid D0 (GPIO16, has no internal pull-up) and D8 (GPIO15, must be low at boot). D3 and D4 must be high at boot.

### Software setup
1. Download and install TiConnect
2. Connect the calculator to the D1 Mini with the 2.5mm plug
3. Connect the D1 Mini to your computer and start the TiConnect software
4. When prompted, select the COM port assigned to your D1 Mini

## Using multiple LEDs as transmission indicators
By default, all indicators use the D1 Mini's single on-board LED (D4 / GPIO2, active low). To see which way data is going, you can change the following definitions to your liking:
- LED_RX_PIN: Indicates the calculator transmitting
- LED_TX_PIN: Indicates the computer transmitting
- LED_RX_TMT_PIN: Indicates a timeout when receiving data from the calculator
- LED_TX_TMT_PIN: Indicates a timeout in the calculator acknowledging a transmission from the computer
- LED_ACTIVE_LOW: Set to `1` for LEDs wired from 3.3V to the pin (like the on-board LED), `0` for LEDs wired from the pin to GND

## Timeouts
The D1 Mini has no BOOT/FLASH button, so to continue after a timeout, press a button wired between D3 and GND. Don't hold it while the board powers up or resets, or the ESP8266 will start in flash mode. If you have trouble with timeouts while reading from or writing to the calculator, you can try changing the following:
- TIMEOUT_RX_US: Receive timeout in microseconds
- TIMEOUT_TX_US: Acknowledge timeout in microseconds
- RETRY_PIN: Pin used as the "continue on timeout" button

## ESP8266-specific notes
- Wi-Fi is turned off at startup, because its interrupts would add jitter to the bit-banged timing.
- The CPU runs at 160 MHz (set in `platformio.ini`), and the link routines are placed in IRAM.
- Timeouts are measured with `micros()`, so they are accurate in real time.
- The watchdog is fed during transfers, and the code yields while waiting for the retry button, so long transfers or timeout pauses don't reset the board.
- Like all ESP8266 boards, the D1 Mini prints some boot messages at 74880 baud when it resets. TiConnect may see these as a few garbage bytes if it is already connected.

## Known issues
- Doesn't work with TILP-II (inherited from the original project)
- Not yet tested on hardware

## License
All files within this repo are released under the GNU GPL V3 License as per the LICENSE file stored in the root of this repo.
