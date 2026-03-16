# Real-Time STM32 ECG Monitor — Hard Real-Time Firmware

A hard real-time ECG monitoring system engineered on the STM32F446RE (Cortex-M4), featuring deterministic 1kHz hardware-synchronized sampling, a custom bare-metal DMA graphics driver, Pan-Tompkins QRS detection, and a safety-critical FreeRTOS architecture. Designed with IEC 62304 Software as a Medical Device lifecycle principles from the ground up.

This project represents the embedded firmware component of a larger real-time arrhythmia detection system. The companion ML model (quantized 1D-CNN for on-device rhythm classification) is documented separately.

---

## Verified Performance

| Metric | Result | Method |
|--------|--------|--------|
| Sampling Rate | 1kHz deterministic | Hardware TIM2 trigger |
| Sampling Jitter | < 0.1µs | Logic analyzer, 10,000 samples |
| Graphics CPU Load | 2% during full redraw | FreeRTOS Idle Task measurement |
| DMA vs HAL Speedup | 400% | Benchmark vs HAL_SPI_Transmit |
| Total Signal Latency | ~18ms end-to-end | Pipeline stage analysis |
| 60Hz Noise Rejection | Verified | FIR filter, pre/post ADC comparison |

> Full verification evidence in [`docs/V&V.md`](docs/V&V.md)

---

## Key Technical Features

### 1. Deterministic 1kHz Sampling
Hardware-triggered ADC via TIM2 — no polling loops, no scheduler dependency.

```
TIM2 Update Event (1ms) → ADC1 Trigger → DMA Transfer → ADC_IRQHandler
```

- Jitter: < 0.1µs (hardware-controlled, verified on logic analyzer)
- Zero CPU overhead during acquisition — CPU not involved until buffer is ready
- Interrupt priority: TIM2 at highest NVIC priority (0) to prevent metronome jitter

### 2. Custom Bare-Metal DMA Graphics Driver
HAL bypassed entirely for the critical rendering path. Direct register access to `DMA2_Stream3` and `SPI1->DR`.

| Implementation | CPU Load | Frame Draw Time | Blocking |
|----------------|----------|-----------------|----------|
| HAL_SPI_Transmit | 98% | 42ms | Yes |
| Bare-Metal DMA | **2%** | **11ms** | **No** |

The `0x2C` memory write command and pixel data are combined into a single atomic SPI transaction via manual CS pin control, eliminating inter-packet delays. This keeps the Algorithm_Task free to process ECG data during screen updates.

### 3. DSP & Signal Processing
- **Filtering:** 17-tap recursive FIR (boxcar) filter tuned to 1kHz sampling rate, creating a spectral null at ~59Hz to reject power line interference while preserving QRS morphology
- **Algorithm:** Pan-Tompkins implementation — squaring, integration, adaptive thresholding — for real-time R-peak detection and R-R interval calculation

### 4. Safety-Critical Architecture
- **Asystole Watchdog:** Monitors inter-beat intervals. If no R-peak detected for >3 seconds, forces `BPM = 0` to prevent stale vital sign display — the highest-severity failure mode identified in the FMEA
- **Thread Safety:** FreeRTOS binary semaphore (`lcdSpiSemaphore`) enforces atomic SPI bus access between DMA ISR and GUI_Task
- **Stack Guards:** FreeRTOS `vApplicationStackOverflowHook` enabled across all tasks

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│              Hard Real-Time Acquisition Layer               │
│                                                             │
│  TIM2 (1kHz) ──► ADC1 (PA0) ──► ADC_IRQHandler              │
│                                       │                     │
│                               ecgQueue (FreeRTOS)           │
└───────────────────────────────────────┼─────────────────────┘
                                        │
┌───────────────────────────────────────▼─────────────────────┐
│              Soft Real-Time Processing Layer                │
│                                                             │
│  Algorithm_Task (High Priority)                             │
│    ├── Recursive FIR Filter (60Hz rejection)                │
│    ├── Pan-Tompkins QRS Detection                           │
│    ├── R-R Interval → BPM Calculation                       │
│    └── Asystole Watchdog (>3s no beat → BPM = 0)            │
└───────────────────────────────────────┬─────────────────────┘
                                        │
┌───────────────────────────────────────▼─────────────────────┐
│              Visualization Layer (Background)               │
│                                                             │
│  GUI_Task (Normal Priority)                                 │
│    ├── Acquire lcdSpiSemaphore                              │
│    ├── DMA2_Stream3 → SPI1 → ILI9341                        │
│    └── Release semaphore on Transfer Complete + BSY clear   │
└─────────────────────────────────────────────────────────────┘
```

Full interrupt priority scheme and memory layout documented in [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

---

## Formal Documentation

This project was developed with IEC 62304 Software as a Medical Device lifecycle principles. The `/docs` directory contains full engineering artifacts:

| Document                                    | Contents |
|---------------------------------------------|----------|
| [`ARCHITECTURE.md`](docs/ARCHITECTURE.md)   | Data flow pipeline, concurrency model, NVIC priority scheme, memory layout |
| [`RISK_ANALYSIS.md`](docs/RISK_ANALYSIS.md) | ISO 14971 FMEA — 5 identified hazards with severity ratings and software mitigations |
| [`TEST_PLAN.md`](docs/TEST_PLAN.md)         | Three-level test strategy (Unit → Integration → System), SOUP management table |
| [`V&V.md`](docs/V&V.md)                     | Verified timing fidelity, DMA benchmark data, latency pipeline analysis, noise rejection results |

---

## Hardware

| Component | Pin | Function |
|-----------|-----|----------|
| STM32F446RE | — | Cortex-M4, target MCU |
| AD8232 AFE | PA0 | ADC1_IN0 — ECG analog signal |
| ILI9341 TFT | PA5 | SPI1_SCK |
| | PA7 | SPI1_MOSI |
| | PB6 | CS (manually controlled) |
| | PC7 | DC (Data/Command) |

---

## Build & Flash

### CLion / CMake
```bash
# Open repository root in CLion
# CMakeLists.txt auto-detects arm-none-eabi-gcc toolchain
# Build → Flash via OpenOCD Run/Debug configuration
```

### STM32CubeIDE
```
File → Import → Existing STM32 Project
Project → Build All
Run As → STM32 Cortex-M C/C++ Application
```

---

## Tech Stack

| Component | Detail |
|-----------|--------|
| Target MCU | STM32F446RE (Cortex-M4) |
| RTOS | FreeRTOS 10.3.1 |
| HAL | STM32 HAL 1.25.0 (bypassed for critical paths) |
| DSP | CMSIS-DSP 1.8.0 |
| Unit Testing | Unity (C) |
| Toolchain | arm-none-eabi-gcc, CMake / STM32CubeIDE |
| Verification | Saleae Logic Analyzer, Oscilloscope |

---

## Project Structure

```
stm32-ecg-monitor/
├── firmware/                   # STM32 firmware source
├── docs/
│   ├── ARCHITECTURE.md         # System block diagram & ISR flows
│   ├── RISK_ANALYSIS.md        # ISO 14971 FMEA
│   ├── TEST_PLAN.md            # Unit/integration/system test strategy
│   └── V&V.md                  # Verification & validation results
├── CMakeLists.txt
└── README.md
```

---

*Author: Andrew V. Franco*