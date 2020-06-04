#include "../include/global.h"
#include "../include/rtc.h"
#include "../include/new/ram_locs.h"

extern u16 sRTCErrorStatus;
extern u8 sRTCProbeResult;
extern u16 sRTCSavedIme;
extern u8 sRTCFrameCount;

extern struct SiiRtcInfo sRtc; //0x203E060

// const rom
static const struct SiiRtcInfo sRtcDummy = {.year = 0, .month = MONTH_JAN, .day = 1}; // 2000 Jan 1

static const s32 sNumDaysInMonths[12] =
{
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
};

void RtcDisableInterrupts(void)
{
	sRTCSavedIme = REG_IME;
	REG_IME = 0;
}

void RtcRestoreInterrupts(void)
{
	REG_IME = sRTCSavedIme;
}

u32 ConvertBcdToBinary(u8 bcd)
{
	if (bcd > 0x9F)
		return 0xFF;

	if ((bcd & 0xF) <= 9)
		return (10 * ((bcd >> 4) & 0xF)) + (bcd & 0xF);
	else
		return 0xFF;
}

static bool8 IsLeapYear(u32 year)
{
	if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))
		return TRUE;

	return FALSE;
}

void RtcInit(void)
{
	sRTCErrorStatus = 0;

	RtcDisableInterrupts();
	SiiRtcUnprotect();
	sRTCProbeResult = SiiRtcProbe();
	RtcRestoreInterrupts();

	if ((sRTCProbeResult & 0xF) != 1)
	{
		sRTCErrorStatus = RTC_INIT_ERROR;
		return;
	}

	if (sRTCProbeResult & 0xF0)
		sRTCErrorStatus = RTC_INIT_WARNING;
	else
		sRTCErrorStatus = 0;

	RtcGetRawInfo(&sRtc);
	sRTCErrorStatus = RtcCheckInfo(&sRtc);
}

u16 RtcGetErrorStatus(void)
{
	return sRTCErrorStatus;
}

void RtcGetInfo(struct SiiRtcInfo *rtc)
{
	if (sRTCErrorStatus & RTC_ERR_FLAG_MASK)
		*rtc = sRtcDummy;
	else
		RtcGetRawInfo(rtc);
}

void RtcGetDateTime(struct SiiRtcInfo *rtc)
{
	RtcDisableInterrupts();
	SiiRtcGetDateTime(rtc);
	RtcRestoreInterrupts();
}

void RtcGetStatus(struct SiiRtcInfo *rtc)
{
	RtcDisableInterrupts();
	SiiRtcGetStatus(rtc);
	RtcRestoreInterrupts();
}

void RtcGetRawInfo(struct SiiRtcInfo *rtc)
{
	RtcGetStatus(rtc);
	RtcGetDateTime(rtc);
}

u16 RtcCheckInfo(struct SiiRtcInfo *rtc)
{
	u16 errorFlags = 0;
	s32 year;
	s32 month;
	s32 value;

	if (rtc->status & SIIRTCINFO_POWER)
		errorFlags |= RTC_ERR_POWER_FAILURE;

	if (!(rtc->status & SIIRTCINFO_24HOUR))
		errorFlags |= RTC_ERR_12HOUR_CLOCK;

	year = ConvertBcdToBinary(rtc->year);

	if (year == 0xFF)
		errorFlags |= RTC_ERR_INVALID_YEAR;

	month = ConvertBcdToBinary(rtc->month);

	if (month == 0xFF || month == 0 || month > 12)
		errorFlags |= RTC_ERR_INVALID_MONTH;

	value = ConvertBcdToBinary(rtc->day);

	if (value == 0xFF)
		errorFlags |= RTC_ERR_INVALID_DAY;

	if (month == MONTH_FEB)
	{
		if (value > IsLeapYear(year) + sNumDaysInMonths[month - 1])
			errorFlags |= RTC_ERR_INVALID_DAY;
	}
	else
	{
		if (value > sNumDaysInMonths[month - 1])
			errorFlags |= RTC_ERR_INVALID_DAY;
	}

	value = ConvertBcdToBinary(rtc->hour);

	if (value > 24)
		errorFlags |= RTC_ERR_INVALID_HOUR;

	value = ConvertBcdToBinary(rtc->minute);

	if (value > 60)
		errorFlags |= RTC_ERR_INVALID_MINUTE;

	value = ConvertBcdToBinary(rtc->second);

	if (value > 60)
		errorFlags |= RTC_ERR_INVALID_SECOND;

	return errorFlags;
}

static void UpdateClockFromRtc(struct SiiRtcInfo *rtc)
{
	Clock->year = ConvertBcdToBinary(rtc->year) + 2000; //Base year is 2000
	Clock->month = ConvertBcdToBinary(rtc->month);
	Clock->day = ConvertBcdToBinary(rtc->day);
	Clock->dayOfWeek = ConvertBcdToBinary(rtc->dayOfWeek);
	Clock->hour = ConvertBcdToBinary(rtc->hour);
	Clock->minute = ConvertBcdToBinary(rtc->minute);
	Clock->second = ConvertBcdToBinary(rtc->second);
}

void RtcCalcLocalTime(void)
{
	if (sRTCFrameCount == 0)
	{
		RtcInit();
		UpdateClockFromRtc(&sRtc);
	}
	else if (sRTCFrameCount >= 60) //Update once a second
	{
		sRTCFrameCount = 0;
		return;
	}

	++sRTCFrameCount;
}

void ForceClockUpdate(void)
{
	sRTCFrameCount = 0;
}