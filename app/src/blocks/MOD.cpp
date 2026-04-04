/**
 * @file MOD.cpp
 * @brief Registers the Zephyr log module used by the MOD template.
 *
 * Created on: Apr 1, 2026
 *     Author: alax
 */


#include <zephyr/logging/log.h>

/** @brief Zephyr log module for the MOD control-surface block. */
LOG_MODULE_REGISTER(MOD, LOG_LEVEL_INF);
