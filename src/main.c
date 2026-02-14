// Could be defined here, or in the processor defines.
#define SYSTEM_CORE_CLOCK 48000000
#define USE_CALC 1

#include "ch32v00x.h"
#if USE_CALC
#include "calibration.h"
#endif

#define APB_CLOCK SYSTEM_CORE_CLOCK
#define ADC_MAX 1023u
#define ADC_MIN_DEFAULT 0
#define ADC_MAX_DEFAULT 1023u
#define UART_ENABLE 0
#define BUTTON_DEBOUNCE_MS 20u
#define BUTTON_LONG_MS 1000u
#define PWM_USE_PC4 1 // 1 = TIM1 CH4 on PC4 (hardware PWM). 0 = TIM2 ISR on PA1.
#define PWM_DUTY_MIN_PERCENT 15u // Clamp output duty lower bound (0..100).
#define PWM_DUTY_MAX_PERCENT 90u // Clamp output duty upper bound (0..100).

#if PWM_USE_PC4
static void SetupPWM_PC4_10k(void)
{
	// Enable GPIOC + AFIO + TIM1 clocks.
	RCC->APB2PCENR |= RCC_IOPCEN | RCC_AFIOEN | RCC_TIM1EN;

	// Reset TIM1 to a known state.
	RCC->APB2PRSTR |= RCC_TIM1RST;
	RCC->APB2PRSTR &= ~RCC_TIM1RST;

	// PC4 as AF push-pull, 50MHz.
	GPIOC->CFGLR &= ~(0xF << (4 * 4));
	GPIOC->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP_AF) << (4 * 4);

	// TIM1 base: 48MHz / (PSC+1) / (ARR+1) = 10kHz
	TIM1->CTLR1 = 0;
	TIM1->CTLR2 = 0;
	TIM1->SMCFGR = 0;
	TIM1->PSC = 47;     // 48MHz / 48 = 1MHz
	TIM1->ATRLR = 99;   // 1MHz / 100 = 10kHz
	TIM1->CH4CVR = 50;  // start at 50% duty

	// PWM mode 1 on CH4, preload enable.
	TIM1->CHCTLR2 &= ~TIM_OC4M;
	TIM1->CHCTLR2 |= TIM_OC4PE | TIM_OC4M_1 | TIM_OC4M_2;
	TIM1->CCER |= TIM_CC4E;

	// Enable main output and start.
	TIM1->BDTR |= TIM_MOE;
	TIM1->SWEVGR = TIM_UG;
	TIM1->CTLR1 |= TIM_ARPE | TIM_CEN;
}

#else
static void SetupPWM_PA1_10k(void)
{
	// Enable GPIOA + TIM2 clocks.
	RCC->APB2PCENR |= RCC_IOPAEN;
	RCC->APB1PCENR |= RCC_TIM2EN;

	// Reset TIM2 to a known state.
	RCC->APB1PRSTR |= RCC_TIM2RST;
	RCC->APB1PRSTR &= ~RCC_TIM2RST;

	// PA1 as GPIO output (toggled in the ISR).
	GPIOA->CFGLR &= ~(0xF << (4 * 1));
	GPIOA->CFGLR |= (GPIO_Speed_50MHz | GPIO_CNF_OUT_PP) << (4 * 1);
	GPIOA->BCR = 1 << 1;

	// TIM2 base: 48MHz / (PSC+1) / (ARR+1) = 10kHz
	TIM2->CTLR1 = 0;
	TIM2->CTLR2 = 0;
	TIM2->SMCFGR = 0;
	TIM2->CHCTLR1 &= ~TIM_CC1S; // CC1 as output compare
	TIM2->PSC = 47;     // 48MHz / 48 = 1MHz
	TIM2->ATRLR = 99;   // 1MHz / 100 = 10kHz
	TIM2->CH1CVR = 50;  // start at 50% duty

	// Enable update + CC1 interrupts.
	TIM2->DMAINTENR |= TIM_UIE | TIM_CC1IE;
	NVIC_EnableIRQ(TIM2_IRQn);

	// Load registers and start.
	TIM2->SWEVGR = TIM_UG;
	TIM2->CTLR1 |= TIM_ARPE | TIM_CEN;
	__enable_irq();
}

void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM2_IRQHandler(void)
{
	uint16_t status = TIM2->INTFR;

	if (status & TIM_UIF)
	{
		TIM2->INTFR &= ~TIM_UIF;
		GPIOA->BSHR = 1 << 1;
	}
	if (status & TIM_CC1IF)
	{
		TIM2->INTFR &= ~TIM_CC1IF;
		GPIOA->BCR = 1 << 1;
	}
}
#endif

static void SetupADC_PA2(void)
{
	// Enable GPIOA + ADC clocks.
	RCC->APB2PCENR |= RCC_IOPAEN | RCC_ADC1EN;

	// ADC clock prescaler: PCLK2 / 6 (48MHz / 6 = 8MHz)
	RCC->CFGR0 &= ~RCC_ADCPRE;
	RCC->CFGR0 |= RCC_ADCPRE_DIV6;

	// PA2 as analog input.
	GPIOA->CFGLR &= ~(0xF << (4 * 2));
	GPIOA->CFGLR |= (GPIO_CNF_IN_ANALOG | GPIO_SPEED_IN) << (4 * 2);

	// Configure ADC for single conversion on channel 0 (PA2 = A0 on SOP-8).
	ADC1->CTLR1 = 0;
	ADC1->CTLR2 = ADC_ADON;
	ADC1->CTLR2 &= ~ADC_EXTSEL;
	ADC1->CTLR2 |= ADC_ExternalTrigConv_None | ADC_EXTTRIG;
	ADC1->RSQR1 = 0;
	ADC1->RSQR2 = 0;
	ADC1->RSQR3 = ADC_Channel_0;

	// Sample time for channel 0: 73 cycles.
	ADC1->SAMPTR2 &= ~ADC_SMP0;
	ADC1->SAMPTR2 |= ADC_SampleTime_73Cycles;

	// Reset and calibrate.
	ADC1->CTLR2 |= ADC_RSTCAL;
	while (ADC1->CTLR2 & ADC_RSTCAL) {}
	ADC1->CTLR2 |= ADC_CAL;
	while (ADC1->CTLR2 & ADC_CAL) {}
}

static void SetupInput_PC1_Pullup(void)
{
	RCC->APB2PCENR |= RCC_IOPCEN;

	// PC1 input with pull-up/down.
	GPIOC->CFGLR &= ~(0xF << (4 * 1));
	GPIOC->CFGLR |= (GPIO_SPEED_IN | GPIO_CNF_IN_PUPD) << (4 * 1);

	GPIOC->BSHR = 1 << 1; // pull-up (active low)
}

static uint16_t ReadADC_PA2(void)
{
	ADC1->CTLR2 |= ADC_SWSTART;
	while (!(ADC1->STATR & ADC_EOC)) {}
	return (uint16_t)(ADC1->RDATAR & 0x03FF);
}

static uint8_t ButtonPressed(void)
{
	return (GPIOC->INDR & (1 << 1)) ? 0u : 1u;
}

int main()
{
	SystemInit48HSI();
#if UART_ENABLE
	SetupUART(UART_BRR);
	// printf("btn test\r\n");
#endif
	#if PWM_USE_PC4
	SetupPWM_PC4_10k();
	#else
	SetupPWM_PA1_10k();
	#endif
	#if USE_CALC
	SetupInput_PC1_Pullup();
	#endif
	SetupADC_PA2();
	#if USE_CALC
	calib_data_t calib = { ADC_MIN_DEFAULT, ADC_MAX_DEFAULT };
	if (!calib_load(&calib) || calib.adc_max <= calib.adc_min)
	{
		calib.adc_min = ADC_MIN_DEFAULT;
		calib.adc_max = ADC_MAX_DEFAULT;
	}
	uint16_t press_ms = 0;
	uint8_t pressed_prev = 0;
	#endif
	
	while(1)
	{
		uint16_t adc_raw = ReadADC_PA2();
		uint16_t adc = adc_raw;
		#if PWM_USE_PC4
		uint16_t arr = TIM1->ATRLR;
		#else
		uint16_t arr = TIM2->ATRLR;
		#endif
#if USE_CALC
		if (adc < calib.adc_min)
			adc = calib.adc_min;
		if (adc > calib.adc_max)
			adc = calib.adc_max;

		uint16_t span = (uint16_t)(calib.adc_max - calib.adc_min);
		uint16_t adj = (uint16_t)(adc - calib.adc_min);
#else
		if (adc < ADC_MIN_DEFAULT)
			adc = ADC_MIN_DEFAULT;
		if (adc > ADC_MAX_DEFAULT)
			adc = ADC_MAX_DEFAULT;

		uint16_t span = (uint16_t)(ADC_MAX_DEFAULT - ADC_MIN_DEFAULT);
		uint16_t adj = (uint16_t)(adc - ADC_MIN_DEFAULT);
#endif
		uint16_t ccr = (uint16_t)((adj * (uint32_t)(arr + 1)) / (span + 1));

		// Clamp mapped duty to user-defined min/max percentages.
		uint16_t ccr_min = (uint16_t)(((arr + 1u) * PWM_DUTY_MIN_PERCENT) / 100u);
		uint16_t ccr_max = (uint16_t)(((arr + 1u) * PWM_DUTY_MAX_PERCENT) / 100u);
		if (ccr_min == 0)
			ccr_min = 1;
		if (ccr_max >= arr)
			ccr_max = arr - 1;
		if (ccr_max <= ccr_min)
			ccr_max = (uint16_t)(ccr_min + 1u);

		if (ccr < ccr_min)
			ccr = ccr_min;
		if (ccr > ccr_max)
			ccr = ccr_max;

		#if PWM_USE_PC4
		TIM1->CH4CVR = ccr;
		#else
		TIM2->CH1CVR = ccr;
		#endif

#if USE_CALC
		uint8_t pressed = ButtonPressed();
		if (pressed)
		{
			if (press_ms < 60000u)
				press_ms = (uint16_t)(press_ms + 5u);
		}
		else if (pressed_prev)
		{
			if (press_ms >= BUTTON_LONG_MS)
			{
				calib.adc_max = adc_raw;
				if (calib.adc_max <= calib.adc_min)
					calib.adc_min = (calib.adc_max > 0) ? (uint16_t)(calib.adc_max - 1) : 0;
				calib_save(&calib);
				// printf("save max=%u\r\n", calib.adc_max);
			}
			else if (press_ms >= BUTTON_DEBOUNCE_MS)
			{
				calib.adc_min = adc_raw;
				if (calib.adc_min >= calib.adc_max)
					calib.adc_max = (uint16_t)(calib.adc_min + 1);
				calib_save(&calib);
				// printf("save min=%u\r\n", calib.adc_min);
			}
			press_ms = 0;
		}
		pressed_prev = pressed;
#endif

		Delay_Ms(5);
	}
}
