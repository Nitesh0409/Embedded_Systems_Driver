# Software UART Using GPIO and Timer Interrupts

## Overview

This project implements UART communication entirely in software without using any hardware UART peripheral. The design is based on GPIO control and precise timing using high-resolution timers in a Linux kernel module.

The implementation demonstrates core embedded systems concepts such as bit-level data transmission, timing synchronization, and interrupt-driven reception.

This implementation is developed as a Linux kernel module and requires Linux kernel headers for compilation.

---

## Features

- Software-based UART implementation (bit-banging)
- Timer-based transmission using high-resolution timers
- Interrupt-driven reception using GPIO edge detection
- Circular buffers for transmit and receive paths
- Character device interface for user-space communication
- Support for multiple UART instances (software-based)

---

## Objectives

- Implement UART protocol using software techniques
- Achieve reliable serial communication using GPIO pins
- Understand timing constraints in asynchronous serial communication
- Use interrupts and timers for precise bit-level sampling

---

## System Flow

```
User Space
   ↓
Write System Call
   ↓
Transmit Buffer
   ↓
High-Resolution Timer (baud timing)
   ↓
GPIO (TX)

GPIO (RX)
   ↓
Interrupt Handler
   ↓
Timer-Based Sampling
   ↓
Receive Buffer
   ↓
User Space Read
```

---

## Working Principle

### Transmission

- Data written from user space is stored in a transmit buffer
- A high-resolution timer generates timing based on a fixed baud rate (9600)
- Each byte is transmitted in the following format:
  - Start bit (0)
  - 8 data bits (LSB first)
  - Stop bit (1)

---

### Reception

- A falling edge on the GPIO pin indicates the start bit
- An interrupt is triggered to initiate reception
- Bits are sampled at mid-bit intervals using a timer
- Received data is stored in a receive buffer for user-space access

---

## Implementation Details

- Implemented as a Linux kernel module
- GPIO used for transmit and receive lines
- High-resolution timers (`hrtimer`) used for precise bit timing
- Interrupt-based reception mechanism
- Character device interface for communication with user space

---

## Project Structure

```
Embedded_Systems_Driver/
│
├── src/
│   └── uart_module/
│       ├── software_uart.c
│       ├── uart_bitbang.c
│       ├── Makefile
│
├── linux/
├── .gitignore
└── README.md
```

---

## Build and Run

### Build

```
cd src/uart_module
make
```

### Load Module

```
sudo insmod software_uart.ko
```

### Device Files

```
/dev/soft_uart1
/dev/soft_uart2
```

---

## Testing

### Terminal 1

```
cat /dev/soft_uart2
```

### Terminal 2

```
echo "Hello" > /dev/soft_uart1
```

---

## Notes

This implementation includes a software loopback mechanism where data written to one UART instance is internally forwarded to another instance for testing without physical wiring.

---

## Conclusion

This project demonstrates a software-based UART implementation using GPIO and timer-driven interrupts. It provides practical understanding of low-level serial communication, timing constraints, and interrupt handling in embedded Linux systems.