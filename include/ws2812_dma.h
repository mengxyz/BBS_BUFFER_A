#pragma once

#include <stdint.h>
#include "ch32v00x.h"

#ifndef WS2812_NR_LEDS
#define WS2812_NR_LEDS 1
#endif

#ifndef WS2812_TIME_SLICES_PER_BIT
#define WS2812_TIME_SLICES_PER_BIT 3
#endif

void ws2812_dma_init(GPIO_TypeDef *port, uint8_t pin_index);
void ws2812_dma_set_rgb(uint8_t r, uint8_t g, uint8_t b);
void ws2812_dma_stop(void);
