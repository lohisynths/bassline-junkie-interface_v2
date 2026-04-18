/**
 * @file VOL.cpp
 * @brief Registers the Zephyr log module used by the VOL block.
 *
 * Created on: Apr 5, 2026
 *     Author: alax
 */

#include <zephyr/logging/log.h>

/** @brief Registers the VOL Zephyr log module. */
LOG_MODULE_REGISTER(VOL, LOG_LEVEL_INF);
