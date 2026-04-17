#ifndef SRC_WAIT_FOR_DSP_H_
#define SRC_WAIT_FOR_DSP_H_

#include "LEDS.h"
#include "UART.h"

/**
 * @brief Waits for the DSP engine ready byte while animating the startup display.
 *
 * Polls UART for the 0xFE byte that signals the DSP engine is ready. While
 * waiting, the LED display shows a simple face animation.
 */
void wait_for_dsp(UART &uart1, LEDSController &leds);

#endif /* SRC_WAIT_FOR_DSP_H_ */
