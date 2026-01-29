# Real-Time STM32 Heart Monitor (Medical-Grade Prototype)

A high-fidelity, hard-real-time ECG monitoring system engineered on the STM32F446RE. This project demonstrates the bridge between **clinical physiology** and **bare-metal firmware**, featuring 1kHz hardware-synchronized sampling, zero-copy DMA graphics, and thread-safe FreeRTOS architecture.

![System Architecture](https://img.shields.io/badge/Architecture-Hybrid%20ISR%2FRTOS-blue)
![Sampling](https://img.shields.io/badge/Sampling-1kHz%20Hardware%20Trigger-green)
![Optimization](https://img.shields.io/badge/Graphics-Bare--Metal%20DMA-red)
![Standard](https://img.shields.io/badge/Standard-ISO%2062304-orange)

## 🩺 Project Scope
* **Objective:** Develop a clinical-grade ECG monitor capable of arrhythmia detection.
* **Hardware:** STM32F446RE (Cortex-M4), AD8232 AFE, ILI9341 TFT.
* **Key Standard:** Designed with **ISO 62304** (Medical Software Lifecycle) principles in mind.

## ⚡ Key Technical Features

### 1. Deterministic 1kHz Sampling (Hardware-Triggered)
Unlike standard polling loops, this system uses **TIM2 (Timer 2)** as a hardware metronome to trigger the ADC exactly every 1000µs.
* **Jitter:** < 1µs (Hardware Controlled).
* **Mechanism:** `TIM2 Update Event` -> `ADC Trigger` -> `DMA Transfer` -> `ISR`.
* **Zero CPU Overhead:** The CPU is not involved until the buffer is ready.

### 2. Custom Bare-Metal DMA Graphics Driver
To achieve 60FPS refresh rates without stalling the heart rate algorithm, I bypassed the HAL (Hardware Abstraction Layer) for the critical rendering path.
* **Implementation:** Direct register access to `DMA2_Stream3` and `SPI1->DR`.
* **Optimization:** Combined `0x2C` (Memory Write) command and Pixel Data into a single atomic SPI transaction by manually controlling the Chip Select (CS) pin, eliminating inter-packet delays.
* **Result:** 400% speedup vs standard HAL_SPI_Transmit.

### 3. DSP & Signal Processing
* **Filtering:** Recursive FIR (Boxcar) filter tuned to 1kHz sampling rate to reject high-frequency noise while preserving QRS complex morphology.
* **Algorithm:** Pan-Tompkins implementation for real-time QRS detection and R-R interval calculation.

### 4. Safety-Critical Architecture
* **Asystole Watchdog (Stale Data Protection):** Implemented a software watchdog that monitors inter-beat intervals. If no R-peak is detected for >3 seconds (Asystole condition), the BPM state is forced to 0 to prevent the display of potentially dangerous "stale" vital signs.
* **Thread Safety:** Inter-task communication via FreeRTOS Binary Semaphores ensures atomic access to the shared SPI bus.
* **Validation:** Signal path validated against 1kHz hardware timing requirements.

## 📂 Repository Structure
```text
/firmware           # STM32 Firmware Source (Main Logic)
/docs           # Verification & Architecture Documentation
  ├── ARCHITECTURE.md       # System Block Diagram & ISR Flows
  ├── VERIFICATION.md       # Logic Analyzer Traces & 1kHz Validation
  └── TEST_PLAN.md          # Unit Test Strategy
```
## 🚀 Build & Flash
This project supports both **CMake** (CLion/VSCode) and **STM32CubeIDE** workflows.

### Option A: CLion / CMake (Recommended)
1.  **Open Project:** Open the repository root in CLion.
2.  **Load CMake:** The `CMakeLists.txt` will automatically detect the `arm-none-eabi-gcc` toolchain.
3.  **Build:** Click **Build** (Hammer icon).
4.  **Flash:** Run the OpenOCD Run/Debug configuration.

### Option B: STM32CubeIDE
1.  **Import:** File -> Import -> Existing STM32 Project.
2.  **Build:** Project -> Build All.
3.  **Debug:** Run As -> STM32 Cortex-M C/C++ Application.

### Hardware Setup
| Component | Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **AD8232 Sensor** | `PA0` | **ADC1_IN0** | Analog Output |
| **ILI9341 Display** | `PA5` | **SPI1_SCK** | Clock |
| | `PA6` | **SPI1_MISO** | (Not used for TX only) |
| | `PA7` | **SPI1_MOSI** | Data Line |
| | `PB6` | **CS** | Chip Select (Manually Controlled) |
| | `PC7` | **DC** | Data/Command |

## 🧪 Verification Status
* [x] **Sampling Fidelity:** Verified 1.000ms delta between samples via GPIO toggle trace.
* [x] **Thread Safety:** Verified Semaphore functionality between `DMA_ISR` and `GUI_Task`.
* [x] **Signal Integrity:** Confirmed 60Hz noise rejection via FIR filter stage.

---
*Author: Andrew V. Franco*