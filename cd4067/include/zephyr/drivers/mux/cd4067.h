#ifndef ZEPHYR_INCLUDE_DRIVERS_MUX_CD4067_H_
#define ZEPHYR_INCLUDE_DRIVERS_MUX_CD4067_H_

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/toolchain/common.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CD4067_CHANNEL_COUNT 16U

__subsystem struct cd4067_driver_api {
	int (*set_channel)(const struct device *dev, uint8_t channel);
	int (*read_raw)(const struct device *dev, int *value);
};

int cd4067_set_channel(const struct device *dev, uint8_t channel);
int cd4067_read_raw(const struct device *dev, int *value);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_DRIVERS_MUX_CD4067_H_ */
