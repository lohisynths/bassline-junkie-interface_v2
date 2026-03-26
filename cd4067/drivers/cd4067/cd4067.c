#define DT_DRV_COMPAT vnd_cd4067_gpio_mux

#include <errno.h>

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/mux/cd4067.h>
#include <zephyr/sys/util.h>

struct cd4067_config {
	struct gpio_dt_spec select[4];
	struct gpio_dt_spec sig;
};

struct cd4067_data {
	uint8_t selected_channel;
};

static int cd4067_set_channel_internal(const struct device *dev, uint8_t channel)
{
	const struct cd4067_config *cfg = dev->config;
	struct cd4067_data *data = dev->data;
	int ret;

	if (channel >= CD4067_CHANNEL_COUNT) {
		return -EINVAL;
	}

	for (size_t i = 0; i < ARRAY_SIZE(cfg->select); ++i) {
		ret = gpio_pin_set_dt(&cfg->select[i], (channel >> i) & 0x1U);
		if (ret < 0) {
			return ret;
		}
	}

	data->selected_channel = channel;

	return 0;
}

static int cd4067_read_raw_internal(const struct device *dev, int *value)
{
	const struct cd4067_config *cfg = dev->config;
	int ret;

	if (value == NULL) {
		return -EINVAL;
	}

	ret = gpio_pin_get_dt(&cfg->sig);
	if (ret < 0) {
		return ret;
	}

	*value = ret;

	return 0;
}

int cd4067_set_channel(const struct device *dev, uint8_t channel)
{
	const struct cd4067_driver_api *api;

	if (dev == NULL) {
		return -EINVAL;
	}

	api = (const struct cd4067_driver_api *)dev->api;
	if ((api == NULL) || (api->set_channel == NULL)) {
		return -ENOSYS;
	}

	return api->set_channel(dev, channel);
}

int cd4067_read_raw(const struct device *dev, int *value)
{
	const struct cd4067_driver_api *api;

	if (dev == NULL) {
		return -EINVAL;
	}

	api = (const struct cd4067_driver_api *)dev->api;
	if ((api == NULL) || (api->read_raw == NULL)) {
		return -ENOSYS;
	}

	return api->read_raw(dev, value);
}

static int cd4067_init(const struct device *dev)
{
	const struct cd4067_config *cfg = dev->config;
	struct cd4067_data *data = dev->data;
	int ret;

	for (size_t i = 0; i < ARRAY_SIZE(cfg->select); ++i) {
		if (!gpio_is_ready_dt(&cfg->select[i])) {
			return -ENODEV;
		}
	}

	if (!gpio_is_ready_dt(&cfg->sig)) {
		return -ENODEV;
	}

	for (size_t i = 0; i < ARRAY_SIZE(cfg->select); ++i) {
		ret = gpio_pin_configure_dt(&cfg->select[i], GPIO_OUTPUT_INACTIVE);
		if (ret < 0) {
			return ret;
		}
	}

	ret = gpio_pin_configure_dt(&cfg->sig, GPIO_INPUT);
	if (ret < 0) {
		return ret;
	}

	data->selected_channel = 0U;

	return 0;
}

#define CD4067_DT_ASSERTS(inst)                                                                  \
	BUILD_ASSERT(DT_INST_PROP_LEN(inst, select_gpios) == 4,                                  \
		     "CD4067 requires exactly four select-gpios entries");                        \
	BUILD_ASSERT(DT_INST_PROP_LEN(inst, sig_gpios) == 1,                                      \
		     "CD4067 requires exactly one sig-gpios entry")

#define CD4067_SELECT_SPEC(inst, idx) GPIO_DT_SPEC_INST_GET_BY_IDX(inst, select_gpios, idx)

#define CD4067_DEFINE(inst)                                                                       \
	CD4067_DT_ASSERTS(inst);                                                                  \
	static const struct cd4067_config cd4067_config_##inst = {                               \
		.select = {                                                                       \
			CD4067_SELECT_SPEC(inst, 0),                                              \
			CD4067_SELECT_SPEC(inst, 1),                                              \
			CD4067_SELECT_SPEC(inst, 2),                                              \
			CD4067_SELECT_SPEC(inst, 3),                                              \
		},                                                                                 \
		.sig = GPIO_DT_SPEC_INST_GET_BY_IDX(inst, sig_gpios, 0),                          \
	};                                                                                        \
	static struct cd4067_data cd4067_data_##inst;                                            \
	static const struct cd4067_driver_api cd4067_api_##inst = {                              \
		.set_channel = cd4067_set_channel_internal,                                      \
		.read_raw = cd4067_read_raw_internal,                                            \
	};                                                                                        \
	DEVICE_DT_INST_DEFINE(inst, cd4067_init, NULL, &cd4067_data_##inst,                     \
			      &cd4067_config_##inst, POST_KERNEL,                                 \
			      CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &cd4067_api_##inst)

DT_INST_FOREACH_STATUS_OKAY(CD4067_DEFINE)
