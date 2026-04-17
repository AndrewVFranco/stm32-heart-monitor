//
// Created by Andrew Franco on 4/16/26.
//
#include "inference.h"
#include "ecg_model.h"
#include "ecg_model_data.h"
#include "ecg_model_data_params.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#define DEBUG_PROFILE_AI 0

static ai_handle network = AI_HANDLE_NULL;
static ai_buffer ai_input[AI_ECG_MODEL_IN_NUM];
static ai_buffer ai_output[AI_ECG_MODEL_OUT_NUM];

AI_ALIGNED(4) static ai_u8 ecg_activations[AI_ECG_MODEL_DATA_ACTIVATIONS_SIZE];

static float compute_energy(float *logits, int n) {
    float max_l = logits[0];
    for (int i = 1; i < n; i++) {
        if (logits[i] > max_l) max_l = logits[i];
    }
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        sum += expf(logits[i] - max_l);
    }
    return -(max_l + logf(sum));
}

uint8_t inference_init(void) {
    ai_network_params params = {
        AI_ECG_MODEL_DATA_WEIGHTS(ai_ecg_model_data_weights_get()),
        AI_ECG_MODEL_DATA_ACTIVATIONS(ecg_activations)
    };

    if (ai_ecg_model_create(&network, AI_ECG_MODEL_DATA_CONFIG).type != AI_ERROR_NONE) {
        return 0; // Creation failed
    }
    if (!ai_ecg_model_init(network, &params)) {
        return 0;
    }

    ai_input[0]  = ai_ecg_model_inputs_get(network, NULL)[0];
    ai_output[0] = ai_ecg_model_outputs_get(network, NULL)[0];

    return 1;
}

RhythmClass_t inference_run(float *circular_buffer, uint16_t head_idx, float *confidence_out) {
    float *input_ptr  = (float *)ai_input[0].data;
    float *output_ptr = (float *)ai_output[0].data;

    // Linearize the circular buffer directly into the AI input buffer
    for (int i = 0; i < ECG_WINDOW_SIZE; i++) {
        input_ptr[i] = circular_buffer[(head_idx + i) % ECG_WINDOW_SIZE];
    }

    float sum = 0.0f;
    for (int i = 0; i < ECG_WINDOW_SIZE; i++) {
        sum += input_ptr[i];
    }
    float mean = sum / ECG_WINDOW_SIZE;

    // Calculate the Standard Deviation
    float var_sum = 0.0f;
    for (int i = 0; i < ECG_WINDOW_SIZE; i++) {
        float diff = input_ptr[i] - mean;
        var_sum += diff * diff; // Squared difference
    }
    float std = sqrtf(var_sum / ECG_WINDOW_SIZE);

    // Normalize the buffer in-place
    for (int i = 0; i < ECG_WINDOW_SIZE; i++) {
        input_ptr[i] = (input_ptr[i] - mean) / (std + 1e-8f);
    }

    // Run the AI Model
    uint32_t start_time = HAL_GetTick();
    ai_i32 n_batch = ai_ecg_model_run(network, ai_input, ai_output);
    uint32_t end_time = HAL_GetTick();

    // Output inference time - Debug
    #if DEBUG_PROFILE_AI
        volatile uint32_t inference_time_ms = end_time - start_time;
        printf("Inference Time: %lu ms\r\n", inference_time_ms);
    #endif

    if (n_batch != 1) return RHYTHM_NONE;

    // Argmax (Find the highest logit)
    int predicted = 0;
    float max_logit = output_ptr[0];
    for (int i = 1; i < AI_ECG_MODEL_OUT_1_SIZE; i++) {
        if (output_ptr[i] > max_logit) {
            predicted = i;
            max_logit = output_ptr[i];
        }
    }

    // Softmax & Energy Calculation
    float sum_exp = 0.0f;
    for (int i = 0; i < AI_ECG_MODEL_OUT_1_SIZE; i++) {
        sum_exp += expf(output_ptr[i] - max_logit);
    }
    float energy = -(max_logit + logf(sum_exp));

    // Debug - Energy
    #if DEBUG_PROFILE_AI
        printf("Current Energy: %.2f\r\n", energy);
    #endif

    // OOD check (Fixed to trigger when Energy is greater than the set threshold)
    if (energy > OOD_ENERGY_THRESHOLD) {
        *confidence_out = 1.0f;
        return RHYTHM_OOD;
    }

    // Calculate actual percentage (0.0 to 1.0)
    // Since max_logit is the predicted class, expf(max_logit - max_logit) = 1.0
    *confidence_out = 1.0f / sum_exp;

    return (RhythmClass_t)predicted;
}

const char* get_rhythm_string(RhythmClass_t r) {
    switch (r) {
#define X(a, b) case a: return b;
        RHYTHM_LIST
#undef X
        default: return "UNKNOWN";
    }
}