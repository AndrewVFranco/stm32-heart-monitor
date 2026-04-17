//
// Created by Andrew Franco on 1/24/26.
//
#include <stdint.h>
#include "heart_rate.h"
#include "PanTompkins.h"

#define BPM_BUFFER_SIZE 5
#define ASYSTOLE_MS 3000
#define ASYSTOLE_PUSH_INTERVAL_MS 1000
#define BPM_MAX 300

static uint32_t bpm_buffer[BPM_BUFFER_SIZE] = {0};
static uint8_t bpm_idx = 0;
static uint32_t last_beat_time = 0;
static uint32_t last_asystole_push = 0;

static uint32_t hr_sum = 0;
static uint8_t hr_count = 0;
static const uint16_t hr_downsample = 5;

extern volatile uint8_t global_bpm;

// HR Debug
// extern volatile uint8_t beat_detected_debug_flag; DEBUG

void Process_HeartRate(uint16_t raw_value, uint32_t current_time) {
    if ((current_time - last_beat_time) > ASYSTOLE_MS) {

        if ((current_time - last_asystole_push) >= ASYSTOLE_PUSH_INTERVAL_MS) {
            // Push '0' into the averaging buffer to drag the average down smoothly when no beats are detected
            bpm_buffer[bpm_idx] = 0;
            bpm_idx = (bpm_idx + 1) % BPM_BUFFER_SIZE;
            last_asystole_push = current_time;

            // Recalculate Global BPM
            uint16_t sum = 0;
            for(int i=0; i<BPM_BUFFER_SIZE; i++) {
                sum += bpm_buffer[i];
            }
            global_bpm = sum / BPM_BUFFER_SIZE;
        }
    }

    hr_sum += raw_value;
    hr_count++;

    if (hr_count >= hr_downsample) {
        uint32_t raw_avg = hr_sum / hr_downsample;
        int16_t clean_val = ((int16_t)raw_avg - 2048) / 2;
        hr_sum = 0;
        hr_count = 0;


        // Pipe the raw ADC value to the library
        int16_t latency_val = PT_StateMachine(clean_val);

        if (latency_val > 0) {
            int16_t result_bpm = PT_get_ShortTimeHR_output(200);

            if (result_bpm > 0 && result_bpm < BPM_MAX) {
                bpm_buffer[bpm_idx] = result_bpm;
                bpm_idx = (bpm_idx + 1) % BPM_BUFFER_SIZE;

                uint16_t sum = 0;
                for(int i=0; i<BPM_BUFFER_SIZE; i++) {
                    sum += bpm_buffer[i];
                }
                global_bpm = sum / BPM_BUFFER_SIZE;

                last_beat_time = current_time;
                last_asystole_push = current_time;
                // beat_detected_debug_flag = 1; DEBUG
            }
        }
    }
}

