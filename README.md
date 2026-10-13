# Capacitance, Inductance & Resistance Meter

A PIC32-based electronic component meter developed in C. The project estimates capacitance, inductance, and resistance from measured electrical signals, displays readings on an LCD, and supports serial communication with a Java desktop application for recording and printing measurements.

## Features

- **Capacitance Measurement:** Estimates capacitance from measured signal frequency, with selectable pF and nF display units.
- **Inductance Measurement:** Calculates inductance from signal frequency, with selectable µH and mH display units.
- **Resistance Measurement:** Uses ADC readings to estimate resistance, with selectable Ω and kΩ display units.
- **Period and Frequency Measurement:** Uses the PIC32 core timer to measure input signal periods and calculate frequency.
- **Temperature Compensation:** Applies a temperature-based correction to capacitance readings using an LM335 temperature sensor.
- **LCD Interface:** Displays measurements and units on a character LCD, with physical switches and buttons for mode selection and data handling.
- **UART Communication:** Transmits measurement records over a serial connection at 115200 baud.
- **Java Desktop Application:** Receives serial data, stores selected measurements, and provides a print dialog for generating measurement records.

## Technical Implementation

- **Firmware:** C with Microchip XC32
- **Microcontroller:** PIC32MX130
- **Desktop Application:** JavaFX, Swing, and jSerialComm
- **Peripherals:** Core timer, ADC, GPIO, UART, and character LCD
- **Key Concepts:** Signal period measurement, frequency-based component estimation, analog-to-digital conversion, temperature compensation, serial communication, and hardware–software integration


