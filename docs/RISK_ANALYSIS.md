# Risk Management File (ISO 14971 Application)

## 1. Risk Analysis Matrix (FMEA)
The following hazards were identified regarding the ECG Monitor firmware. Mitigations have been implemented via software controls.

| ID | Component | Failure Mode | Clinical Effect | Severity | Software Control (Mitigation) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **R1** | Algorithm | **Stale Data** (Patient asystole or lead-off not detected) | Display shows old heart rate (e.g., 80 BPM) while patient is in cardiac arrest. | **High** | **Asystole Watchdog:** Forces `BPM=0` if no R-peak detected for >3 seconds. |
| **R2** | Graphics | **SPI Race Condition** (DMA vs Task) | Screen corruption or frozen display; Vitals unreadable. | **Med** | **Mutex Semaphore:** `lcdSpiSemaphore` enforces atomic access to the SPI bus. |
| **R3** | RTOS | **Priority Inversion** | Algorithm task starved by high-load graphics rendering. | **Med** | **DMA Offload:** Graphics rendering moved to DMA controller; CPU remains free for ECG processing. |
| **R4** | Signal | **60Hz Interference** | Artifacts masked as R-peaks (False Positive Tachycardia). | **Med** | **FIR Notch Filter:** Recursive averager tuned to 1kHz sampling rate (16.6ms window) cancels 60Hz noise. |
| **R5** | Memory | **Stack Overflow** | System crash / Hard Fault during operation. | **High** | **Stack Guards:** Enabled FreeRTOS stack overflow checking hook (`vApplicationStackOverflowHook`). |

## 2. Risk Definitions
* **High Severity:** Failure results in death or serious injury (e.g., missed cardiac arrest).
* **Medium Severity:** Failure results in reversible injury or delay in treatment (e.g., unreadable screen).
* **Low Severity:** Inconvenience (e.g., minor artifact).

## 3. Conclusion
All identified "High" severity risks have been mitigated by specific software architectural controls (Watchdogs, Stack Guards). The residual risk is considered acceptable for a prototype environment.