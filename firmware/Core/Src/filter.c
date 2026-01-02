//
// Created by Andrew Franco on 1/2/26.
//

#include "filter.h"
#include <stdint.h>

uint16_t filter_buffer[FILTER_SIZE] = {0}; // The circular buffer
uint8_t filter_index = 0;                  // Current position in the buffer
uint32_t filter_sum = 0;

uint16_t Filter_Signal(uint16_t new_val) {
    // Subtract the oldest value (at current index) from the sum
    filter_sum -= filter_buffer[filter_index];

    // Overwrite that spot with the NEW value
    filter_buffer[filter_index] = new_val;

    // Add the new value to the sum
    filter_sum += new_val;

    // Move the index forward
    filter_index++;

    // Wrap around if we hit the end of the buffer
    if (filter_index >= FILTER_SIZE) {
        filter_index = 0;
    }

    // Return the average
    return filter_sum / FILTER_SIZE;
}