#include "ws2812_dma.h"

#define WS2812_BITS_PER_LED 24u
#define WS2812_RESET_FRAMES 3u

static GPIO_TypeDef *ws2812_port;
static uint8_t ws2812_pin;
static uint32_t ws2812_hi;
static uint32_t ws2812_lo;
static uint32_t ws2812_colors[WS2812_NR_LEDS];
static uint32_t ws2812_dma_buffer[WS2812_BITS_PER_LED * WS2812_TIME_SLICES_PER_BIT * 2];
static int ws2812_ledno;

static void ws2812_enable_gpio_clock(GPIO_TypeDef *port)
{
	if (port == GPIOA)
		RCC->APB2PCENR |= RCC_IOPAEN;
#ifdef GPIOB
	else if (port == GPIOB)
		RCC->APB2PCENR |= RCC_IOPBEN;
#endif
	else if (port == GPIOC)
		RCC->APB2PCENR |= RCC_IOPCEN;
	else if (port == GPIOD)
		RCC->APB2PCENR |= RCC_IOPDEN;
}

static void ws2812_config_pin(GPIO_TypeDef *port, uint8_t pin_index)
{
	if (pin_index < 8)
	{
		port->CFGLR &= ~(0xF << (4 * pin_index));
		port->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * pin_index);
	}
	else
	{
		uint8_t shift = (uint8_t)(pin_index - 8);
		port->CFGHR &= ~(0xF << (4 * shift));
		port->CFGHR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * shift);
	}
}

static void ws2812_fill(uint32_t *buffer)
{
	const uint32_t hi = ws2812_hi;
	const uint32_t lo = ws2812_lo;

	if (ws2812_ledno >= WS2812_NR_LEDS)
	{
		uint32_t *end = buffer + (WS2812_BITS_PER_LED * WS2812_TIME_SLICES_PER_BIT);
		while (buffer != end)
			*buffer++ = lo;

		ws2812_ledno++;
		if (ws2812_ledno > (WS2812_NR_LEDS + WS2812_RESET_FRAMES))
			ws2812_ledno = 0;
		return;
	}

	uint32_t color = ws2812_colors[ws2812_ledno] & 0x00FFFFFFu;
	for (int bit = 23; bit >= 0; --bit)
	{
		uint32_t val = (color & (1u << bit)) ? hi : lo;
#if WS2812_TIME_SLICES_PER_BIT == 4
		*buffer++ = hi;
		*buffer++ = val;
		*buffer++ = val;
		*buffer++ = lo;
#elif WS2812_TIME_SLICES_PER_BIT == 3
		*buffer++ = hi;
		*buffer++ = val;
		*buffer++ = lo;
#else
#error Unsupported WS2812_TIME_SLICES_PER_BIT value.
#endif
	}

	ws2812_ledno++;
}

void DMA1_Channel2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void DMA1_Channel2_IRQHandler(void)
{
	const uint32_t half = (uint32_t)(sizeof(ws2812_dma_buffer) / sizeof(ws2812_dma_buffer[0]) / 2u);
	uint32_t flags = DMA1->INTFR;

	while (flags)
	{
		DMA1->INTFCR = DMA1_IT_GL2;

		if (flags & DMA1_IT_TC2)
			ws2812_fill(ws2812_dma_buffer + half);
		if (flags & DMA1_IT_HT2)
			ws2812_fill(ws2812_dma_buffer);

		flags = DMA1->INTFR;
	}
}

void ws2812_dma_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
	if (WS2812_NR_LEDS == 0)
		return;

	ws2812_colors[0] = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
}

void ws2812_dma_stop(void)
{
	DMA1_Channel2->CFGR &= ~DMA_CFGR1_EN;
	DMA1->INTFCR = DMA1_IT_GL2;
	TIM1->CTLR1 &= ~TIM_CEN;
	TIM1->BDTR &= ~TIM_MOE;
}

void ws2812_dma_init(GPIO_TypeDef *port, uint8_t pin_index)
{
	ws2812_port = port;
	ws2812_pin = pin_index;
	ws2812_hi = 1u << pin_index;
	ws2812_lo = 1u << (pin_index + 16u);
	ws2812_ledno = 0;

	ws2812_enable_gpio_clock(port);
	ws2812_config_pin(port, pin_index);

	RCC->AHBPCENR |= RCC_DMA1EN | RCC_SRAMEN;
	RCC->APB2PCENR |= RCC_TIM1EN;

	ws2812_fill(ws2812_dma_buffer);
	ws2812_fill(ws2812_dma_buffer + (sizeof(ws2812_dma_buffer) / sizeof(ws2812_dma_buffer[0]) / 2u));

	DMA1_Channel2->CFGR = 0;
	DMA1_Channel2->CNTR = (uint16_t)(sizeof(ws2812_dma_buffer) / sizeof(ws2812_dma_buffer[0]));
	DMA1_Channel2->MADDR = (uint32_t)ws2812_dma_buffer;
	DMA1_Channel2->PADDR = (uint32_t)&ws2812_port->BSHR;
	DMA1_Channel2->CFGR =
		DMA_CFGR1_DIR |
		DMA_CFGR1_PL |
		DMA_CFGR1_MSIZE_1 |
		DMA_CFGR1_PSIZE_1 |
		DMA_CFGR1_MINC |
		DMA_CFGR1_CIRC |
		DMA_CFGR1_HTIE |
		DMA_CFGR1_TCIE |
		DMA_CFGR1_EN;

	DMA1->INTFCR = DMA1_IT_GL2;
	NVIC_EnableIRQ(DMA1_Channel2_IRQn);

	RCC->APB2PRSTR |= RCC_TIM1RST;
	RCC->APB2PRSTR &= ~RCC_TIM1RST;

	TIM1->CTLR1 = 0;
	TIM1->CTLR2 = 0;
	TIM1->SMCFGR = 0;
	TIM1->PSC = 0x0000;
#if WS2812_TIME_SLICES_PER_BIT == 4
	TIM1->ATRLR = 15;
#elif WS2812_TIME_SLICES_PER_BIT == 3
	TIM1->ATRLR = 17;
#else
#error Unsupported WS2812_TIME_SLICES_PER_BIT value.
#endif
	TIM1->SWEVGR = TIM_UG | TIM_TG;
	TIM1->CCER = TIM_CC1E | TIM_CC1P;
	TIM1->CHCTLR1 = TIM_OC1M_2 | TIM_OC1M_1;
	TIM1->CH1CVR = 6;
	TIM1->BDTR = TIM_MOE;
	TIM1->DMAINTENR = TIM_UDE | TIM_CC1DE;
	TIM1->CTLR1 = TIM_CEN;
}
