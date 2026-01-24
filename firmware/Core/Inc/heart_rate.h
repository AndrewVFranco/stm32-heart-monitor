//
// Created by Andrew Franco on 1/24/26.
//

#ifndef STM32_HEART_MONITOR_HEART_RATE_H
#define STM32_HEART_MONITOR_HEART_RATE_H
#include <stdint.h>

void Process_HeartRate(uint16_t raw_value, uint32_t current_time);

#endif //STM32_HEART_MONITOR_HEART_RATE_H