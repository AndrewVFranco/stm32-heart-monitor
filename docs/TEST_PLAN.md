# Software Test Plan (STP)

## 1. Scope
This document defines the testing strategy for the STM32 ECG Monitor firmware. The testing goal is to verify that "Essential Performance" (IEC 60601-1) is maintained under all operating conditions.

## 2. Testing Levels

### Level 1: Unit Testing (Algorithm Validation)
* **Tool:** Unity (C Mocking Framework) / Python Scripting
* **Scope:** Pure logic functions with no hardware dependencies.
* **Key Tests:**
    * **Pan-Tompkins:** Feed known ECG arrays (MIT-BIH data) and assert correct R-peak index detection.
    * **FIR Filter:** Input a Step Function (0 -> 1000) and verify the output settles to 1000 within `FILTER_SIZE` samples.
    * **Watchdog:** Input a timestamp delta of 3001ms and assert `BPM == 0`.

### Level 2: Integration Testing (Hardware-Software Interface)
* **Scope:** Interaction between ISRs, DMA, and RTOS Tasks.
* **Key Tests:**
    * **SPI Race Condition:** Rapidly update `HeartRate` global variable while Screen DMA is active. *Pass Criteria: No visual artifacts or Hard Faults.*
    * **Queue Overflow:** Force ADC to generate data faster than Algorithm Task can process. *Pass Criteria: System detects overflow and sets Error Flag (Fail-Safe).*

### Level 3: System Verification (Validation)
* **Tool:** Saleae Logic Analyzer & Oscilloscope.
* **Scope:** End-to-end latency and jitter analysis.
* **Reference:** See `VERIFICATION_REPORT.md` for results.

## 3. SOUP (Software of Unknown Provenance) Management
The following third-party libraries are used. They are segregated from safety-critical logic where possible.

| SOUP Name | Version | Usage | Risk Mitigation |
| :--- | :--- | :--- | :--- |
| **FreeRTOS** | 10.3.1 | Task Scheduling | Enable Stack Overflow Detection (`configCHECK_FOR_STACK_OVERFLOW`). |
| **STM32 HAL** | 1.25.0 | Hardware Init | Bypassed for critical paths (ADC/DMA) to reduce "Black Box" risk. |
| **CMSIS-DSP** | 1.8.0 | Math | Unit tested against Python reference implementation. |

