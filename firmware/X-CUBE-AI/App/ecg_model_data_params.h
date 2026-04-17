/**
  ******************************************************************************
  * @file    ecg_model_data_params.h
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-04-16T14:51:11-0700
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */

#ifndef ECG_MODEL_DATA_PARAMS_H
#define ECG_MODEL_DATA_PARAMS_H

#include "ai_platform.h"

/*
#define AI_ECG_MODEL_DATA_WEIGHTS_PARAMS \
  (AI_HANDLE_PTR(&ai_ecg_model_data_weights_params[1]))
*/

#define AI_ECG_MODEL_DATA_CONFIG               (NULL)


#define AI_ECG_MODEL_DATA_ACTIVATIONS_SIZES \
  { 94784, }
#define AI_ECG_MODEL_DATA_ACTIVATIONS_SIZE     (94784)
#define AI_ECG_MODEL_DATA_ACTIVATIONS_COUNT    (1)
#define AI_ECG_MODEL_DATA_ACTIVATION_1_SIZE    (94784)



#define AI_ECG_MODEL_DATA_WEIGHTS_SIZES \
  { 65420, }
#define AI_ECG_MODEL_DATA_WEIGHTS_SIZE         (65420)
#define AI_ECG_MODEL_DATA_WEIGHTS_COUNT        (1)
#define AI_ECG_MODEL_DATA_WEIGHT_1_SIZE        (65420)



#define AI_ECG_MODEL_DATA_ACTIVATIONS_TABLE_GET() \
  (&g_ecg_model_activations_table[1])

extern ai_handle g_ecg_model_activations_table[1 + 2];



#define AI_ECG_MODEL_DATA_WEIGHTS_TABLE_GET() \
  (&g_ecg_model_weights_table[1])

extern ai_handle g_ecg_model_weights_table[1 + 2];


#endif    /* ECG_MODEL_DATA_PARAMS_H */
