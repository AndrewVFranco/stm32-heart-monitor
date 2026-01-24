//
// Created by Andrew Franco on 1/24/26.
//

#include <stdint.h>
#include "adc.h"

uint16_t AD8232_Read() {
    return HAL_ADC_GetValue(&hadc1);
}
