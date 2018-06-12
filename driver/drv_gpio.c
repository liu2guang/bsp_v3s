#include "rthw.h"
#include <rtthread.h>
#include "rtdevice.h"

#include "board.h"
#include "interrupt.h"

void pin_write(rt_device_t dev, rt_base_t pin, rt_base_t value)
{
    gpio_set_value(pin, value);
}

int pin_read(rt_device_t dev, rt_base_t pin)
{
	return gpio_get_value(pin);
}

void pin_mode(rt_device_t dev, rt_base_t pin, rt_base_t mode)
{
    if (mode == PIN_MODE_OUTPUT){
        gpio_set_mode(pin, PIN_TYPE(SUNXI_GPIO_OUTPUT)|PULL_UP);
    }else if(mode == PIN_MODE_OUTPUT_OD){
        gpio_set_mode(pin, PIN_TYPE(SUNXI_GPIO_OUTPUT)|PULL_NO);
    }else{
        gpio_set_mode(pin, PIN_TYPE(SUNXI_GPIO_INPUT)|PULL_UP);
    }
}

const static struct rt_pin_ops _pin_ops = 
{
	pin_mode,
    pin_write,
    pin_read,
};

int rt_hw_pin_init(void)
{
	rt_device_pin_register("pin", &_pin_ops, RT_NULL);
	return 0;
}
INIT_DEVICE_EXPORT(rt_hw_pin_init);