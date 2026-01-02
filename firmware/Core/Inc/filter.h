//
// Created by Andrew Franco on 1/2/26.
//

#ifndef STM32_HEART_MONITOR_FILTER_H
#define STM32_HEART_MONITOR_FILTER_H

#include <stdint.h>

#define FILTER_SIZE 33
#define SAMPLE_RATE_MS 2

uint16_t Filter_Signal(uint16_t new_val);

#endif //STM32_HEART_MONITOR_FILTER_H