/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file led.h
 * @brief LED module header
 */

#ifndef LED_H
#define LED_H

#include <zephyr/kernel.h>
#include <stdint.h>

/**
 * @brief Initialize LED module
 * @return 0 on success, negative error code on failure
 */
int led_module_init(void);

/**
 * @brief Get LED state
 * @param led_number LED number (1-4)
 * @param state Pointer to store LED state
 * @return 0 on success, negative error code on failure
 */
int led_get_state(uint8_t led_number, bool *state);

/**
 * @brief Get all LED states as JSON
 * @param buf Buffer to store JSON string
 * @param buf_len Buffer length
 * @return Number of bytes written, or negative error code
 */
int led_get_all_states_json(char *buf, size_t buf_len);

#endif /* LED_H */
