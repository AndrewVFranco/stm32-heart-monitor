# System Architecture

## High-Level Overview
The system employs a **Hybrid Event-Driven Architecture**, strictly separating **Hard Real-Time** acquisition (handled by hardware interrupts) from **Soft Real-Time** signal processing (handled by FreeRTOS tasks).

### Data Flow Pipeline
The signal path is designed to minimize CPU intervention using DMA and Hardware Triggers.

1.  **Acquisition Stage (Hard Real-Time)**
    * **Source:** `TIM2` (1kHz Hardware Metronome).
    * **Trigger:** `TIM2_TRGO` event fires on Update (1ms).
    * **Conversion:** `ADC1` converts analog signal (PA0) to 12-bit int.
    * **Transport:** `ADC_IRQHandler` captures data immediately upon End of Conversion (EOC).
    * **Output:** Raw data pushed to `ecgQueue` (FreeRTOS Queue).

2.  **Processing Stage (Soft Real-Time)**
    * **Task:** `Algorithm_Task` (Priority: High).
    * **Input:** Blocks on `ecgQueue` receive.
    * **Filtering:** Recursive FIR (Moving Average) to reject 60Hz noise.
    * **Analysis:** Pan-Tompkins Algorithm (Squaring -> Integration -> Adaptive Thresholding).
    * **Output:** Updates global `HeartRate` state and `FilterOut` display buffer.

3.  **Visualization Stage (Background)**
    * **Task:** `GUI_Task` (Priority: Normal).
    * **Mechanism:** Zero-Copy Bare-Metal DMA.
    * **Synchronization:** Acquires `lcdSpiSemaphore` before drawing.
    * **Action:** Configures `DMA2_Stream3` to blast pixel data to ILI9341 via SPI1.

---

## Concurrency & Synchronization
Safety-critical systems require robust protection against Race Conditions.

### 1. The SPI Bus Mutex (`lcdSpiSemaphore`)
The ILI9341 display shares the SPI bus. To prevent the Algorithm task or other peripherals from interrupting a massive DMA transfer, we implement a Binary Semaphore.
* **Take:** `GUI_Task` takes the semaphore before starting a frame.
* **Give:** `DMA2_Stream3_IRQHandler` gives the semaphore back only when the Transfer Complete (TC) flag is set **AND** the SPI Busy (BSY) flag is cleared.

### 2. The Asystole Watchdog
A software watchdog monitors the time delta between detected R-peaks.
* **Condition:** `(CurrentTime - LastBeatTime) > 3000ms`.
* **Action:** Force `BPM = 0`.
* **Rationale:** Prevents "Stale Data" hazards where the screen displays a healthy heart rate despite patient cardiac arrest or lead detachment.

---

## Interrupt Priority Scheme (NVIC)
To ensure acquisition fidelity, priorities are tiered:

| IRQ | Priority | Rationale |
| :--- | :--- | :--- |
| **TIM2_IRQn** | 0 (Highest) | Metronome cannot jitter. |
| **ADC_IRQn** | 1 | Data capture must happen immediately. |
| **DMA2_Stream3** | 5 | Graphics completion is less critical than acquisition. |
| **SysTick** | 15 (Lowest) | OS Scheduler yields to hardware. |

---

## Memory Layout
* **ADC Buffer:** `uint16_t` (Single sample depth, ISR-managed).
* **Filter Buffer:** `int16_t[FILTER_SIZE]` (Circular buffer for FIR).
* **Display Buffer:** `uint16_t[320]` (One scanline, DMA-accessible SRAM).