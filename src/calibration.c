#include "calibration.h"
#include "ch32v00x.h"
#include <stddef.h>

#define CALIB_MAGIC 0x43414C31u // "CAL1"
#define FLASH_SIZE_BYTES 0x4000u
#define FLASH_PAGE_SIZE 1024u
#define CALIB_FLASH_ADDR (FLASH_BASE + FLASH_SIZE_BYTES - FLASH_PAGE_SIZE)

typedef struct __attribute__((packed))
{
	uint32_t magic;
	uint16_t version;
	uint16_t length;
	uint16_t adc_min;
	uint16_t adc_max;
	uint16_t checksum;
	uint16_t reserved;
} calib_record_t;

static uint16_t calib_checksum(const calib_record_t *rec)
{
	const uint8_t *bytes = (const uint8_t *)rec;
	uint16_t sum = 0;
	const uint16_t chk_off = (uint16_t)offsetof(calib_record_t, checksum);
	for (uint16_t i = 0; i < (uint16_t)sizeof(calib_record_t); i++)
	{
		if (i == chk_off || i == (uint16_t)(chk_off + 1u))
			continue;
		sum = (uint16_t)(sum + bytes[i]);
	}
	return sum;
}

static void flash_wait_ready(void)
{
	while (FLASH->STATR & FLASH_STATR_BSY) {}
}

static void flash_unlock(void)
{
	if (FLASH->CTLR & FLASH_CTLR_LOCK)
	{
		FLASH->KEYR = FLASH_KEY1;
		FLASH->KEYR = FLASH_KEY2;
	}
}

static void flash_lock(void)
{
	FLASH->CTLR |= FLASH_CTLR_LOCK;
}

static void flash_clear_flags(void)
{
	FLASH->STATR = FLASH_STATR_EOP | FLASH_STATR_WRPRTERR;
}

uint8_t calib_load(calib_data_t *out)
{
	const calib_record_t *rec = (const calib_record_t *)CALIB_FLASH_ADDR;

	if (rec->magic != CALIB_MAGIC)
		return 0;
	if (rec->length != (uint16_t)sizeof(calib_record_t))
		return 0;
	if (rec->checksum != calib_checksum(rec))
		return 0;

	out->adc_min = rec->adc_min;
	out->adc_max = rec->adc_max;
	return 1;
}

uint8_t calib_save(const calib_data_t *in)
{
	calib_record_t rec;
	rec.magic = CALIB_MAGIC;
	rec.version = 1;
	rec.length = (uint16_t)sizeof(calib_record_t);
	rec.adc_min = in->adc_min;
	rec.adc_max = in->adc_max;
	rec.reserved = 0xFFFFu;
	rec.checksum = calib_checksum(&rec);

	const calib_record_t *existing = (const calib_record_t *)CALIB_FLASH_ADDR;
	if (existing->magic == rec.magic &&
		existing->length == rec.length &&
		existing->adc_min == rec.adc_min &&
		existing->adc_max == rec.adc_max &&
		existing->checksum == rec.checksum)
	{
		return 1;
	}

	flash_unlock();
	flash_wait_ready();
	flash_clear_flags();

	FLASH->CTLR |= FLASH_CTLR_PER;
	FLASH->ADDR = CALIB_FLASH_ADDR;
	FLASH->CTLR |= FLASH_CTLR_STRT;
	flash_wait_ready();
	FLASH->CTLR &= ~FLASH_CTLR_PER;

	FLASH->CTLR |= FLASH_CTLR_PG;
	volatile uint16_t *dst = (volatile uint16_t *)CALIB_FLASH_ADDR;
	const uint16_t *src = (const uint16_t *)&rec;
	for (uint16_t i = 0; i < (uint16_t)(sizeof(calib_record_t) / 2u); i++)
	{
		dst[i] = src[i];
		flash_wait_ready();
		if (dst[i] != src[i])
		{
			FLASH->CTLR &= ~FLASH_CTLR_PG;
			flash_lock();
			return 0;
		}
	}
	FLASH->CTLR &= ~FLASH_CTLR_PG;
	flash_lock();

	return 1;
}
