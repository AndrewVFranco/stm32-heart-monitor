//
// Created by Andrew Franco on 4/16/26.
//

#ifndef STM32_HEART_MONITOR_INFERENCE_H
#define STM32_HEART_MONITOR_INFERENCE_H

#include <stdint.h>

#define RHYTHM_LIST \
X(RHYTHM_NORM, "1ST DEG BLOCK ") \
X(RHYTHM_AFIB, "A-FIB         ") \
X(RHYTHM_1DEG, "NORMAL SINUS  ") \
X(RHYTHM_OOD,  "UNRECOGNIZED  ") \
X(RHYTHM_NONE, "NONE          ")

typedef enum {
#define X(a, b) a,
    RHYTHM_LIST
#undef X
} RhythmClass_t;

#define ECG_WINDOW_SIZE      1000
#define OOD_ENERGY_THRESHOLD -3.0f

uint8_t inference_init(void);
RhythmClass_t inference_run(float *circular_buffer, uint16_t head_idx, float *confidence_out);
const char* get_rhythm_string(RhythmClass_t r);

#endif //STM32_HEART_MONITOR_INFERENCE_H