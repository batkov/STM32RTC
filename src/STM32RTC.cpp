/**
  ******************************************************************************
  * @file    STM32RTC.cpp
  * @author  Frederic Pillon
  * @brief   Provides a RTC interface for Arduino
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2020 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

#include <time.h>

#include "STM32RTC.h"

#define EPOCH_TIME_OFF      946684800  // This is 1st January 2000, 00:00:00 in epoch time
#define EPOCH_TIME_YEAR_OFF 100        // years since 1900

// Initialize static variable
bool STM32RTC::_timeSet = false;

/**
  * @brief initializes the RTC
  * @param format: hour format: HOUR_12 or HOUR_24(default)
  * @retval None
  */
void STM32RTC::begin(Hour_Format format)
{
  begin(false, format);
}

/**
  * @brief initializes the RTC
  * @param resetTime: if true reconfigures the RTC
  * @param format: hour format: HOUR_12 or HOUR_24(default)
  * @retval None
  */
void STM32RTC::begin(bool resetTime, Hour_Format format)
{
  bool reinit;

  if (resetTime == true) {
    _timeSet = false;
  }

  _format = format;
  reinit = RTC_init((format == HOUR_12) ? HOUR_FORMAT_12 : HOUR_FORMAT_24,
                    (_clockSource == LSE_CLOCK) ? ::LSE_CLOCK :
                    (_clockSource == HSE_CLOCK) ? ::HSE_CLOCK : ::LSI_CLOCK
                    , resetTime);

  if (reinit == true) {
    _timeSet = false;
    syncTime();
    syncDate();
    // Use current time to init alarm members
    _alarmDay  = _day;
    _alarmHours = _hours;
    _alarmMinutes = _minutes;
    _alarmSeconds = _seconds;
    _alarmSubSeconds = _subSeconds;
    _alarmPeriod = _hoursPeriod;
  } else {
    _timeSet = true;
  }
}

/**
  * @brief Deinitialize and stop the RTC
  * @param None
  * @retval None
  */
void STM32RTC::end(void)
{
  RTC_DeInit();
  _timeSet = false;
}

/**
  * @brief get the RTC clock source.
  * @retval clock source: LSI_CLOCK, LSE_CLOCK or HSE_CLOCK
  */
STM32RTC::Source_Clock STM32RTC::getClockSource(void)
{
  return _clockSource;
}

/**
  * @brief set the RTC clock source. By default LSI clock is selected. This
  * method must be called before begin().
  * @param source: clock source: LSI_CLOCK, LSE_CLOCK or HSE_CLOCK
  * @retval None
  */
void STM32RTC::setClockSource(Source_Clock source)
{
  if (IS_CLOCK_SOURCE(source)) {
    _clockSource = source;
    RTC_SetClockSource((_clockSource == LSE_CLOCK) ? ::LSE_CLOCK :
                       (_clockSource == HSE_CLOCK) ? ::HSE_CLOCK : ::LSI_CLOCK);
  }
}

#if defined(STM32F1xx)
/**
  * @brief  get user asynchronous prescaler value for the current clock source.
  * @param  predivA: pointer to the current Asynchronous prescaler value
  * @param  dummy : not used (kept for compatibility reason)
  * @retval None
  */
void STM32RTC::getPrediv(uint32_t *predivA, int16_t *dummy)
{
  UNUSED(dummy);
  RTC_getPrediv(predivA);
}
#else
/**
  * @brief  get user (a)synchronous prescaler values if set else computed
  *         ones for the current clock source.
  * @param  predivA: pointer to the current Asynchronous prescaler value
  * @param  predivS: pointer to the current Synchronous prescaler value
  * @retval None
  */
void STM32RTC::getPrediv(int8_t *predivA, int16_t *predivS)
{
  if ((predivA != nullptr) && (predivS != nullptr)) {
    RTC_getPrediv(predivA, predivS);
  }
}
#endif /* STM32F1xx */

#if defined(STM32F1xx)
/**
  * @brief  set user asynchronous prescalers value.
  * @note   This method must be called before begin().
  * @param  predivA: Asynchronous prescaler value. Reset value: RTC_AUTO_1_SECOND
  * @param  dummy : not used (kept for compatibility reason)
  * @retval None
  */
void STM32RTC::setPrediv(uint32_t predivA, int16_t dummy)
{
  UNUSED(dummy);
  RTC_setPrediv(predivA);
}
#else
/**
  * @brief  set user (a)synchronous prescalers value.
  * @note   This method must be called before begin().
  * @param  predivA: Asynchronous prescaler value. Reset value: -1
  * @param  predivS: Synchronous prescaler value. Reset value: -1
  * @retval None
  */
void STM32RTC::setPrediv(int8_t predivA, int16_t predivS)
{
  RTC_setPrediv(predivA, predivS);
}
#endif /* STM32F1xx */

/**
  * @brief enable the RTC alarm.
  * @param match: Alarm_Match configuration
  * @retval None
  */
void STM32RTC::enableAlarm(Alarm_Match match)
{
  _alarmMatch = match;
  switch (match) {
    case MATCH_OFF:
      RTC_StopAlarm();
      break;
    case MATCH_YYMMDDHHMMSS://kept for compatibility
    case MATCH_MMDDHHMMSS:  //kept for compatibility
    case MATCH_DHHMMSS:
    case MATCH_HHMMSS:
    case MATCH_MMSS:
    case MATCH_SS:
      RTC_StartAlarm(_alarmDay, _alarmHours, _alarmMinutes, _alarmSeconds,
                     _alarmSubSeconds, (_alarmPeriod == AM) ? HOUR_AM : HOUR_PM,
                     static_cast<uint8_t>(_alarmMatch));
      break;
    default:
      break;
  }
}

/**
  * @brief disable the RTC alarm.
  * @retval None
  */
void STM32RTC::disableAlarm(void)
{
  RTC_StopAlarm();
}

/**
  * @brief attach a callback to the RTC alarm interrupt.
  * @param callback: pointer to the callback
  * @retval None
  */
void STM32RTC::attachInterrupt(voidFuncPtr callback, void *data)
{
  attachAlarmCallback(callback, data);
}

/**
  * @brief detach the RTC alarm callback.
  * @retval None
  */
void STM32RTC::detachInterrupt(void)
{
  detachAlarmCallback();
}

#ifdef ONESECOND_IRQn
/**
  * @brief attach a callback to the RTC Seconds interrupt.
  * @param callback: pointer to the callback
  * @retval None
  */
void STM32RTC::attachSecondsInterrupt(voidFuncPtr callback)
{
  attachSecondsIrqCallback(callback);
}

/**
  * @brief detach the RTC Seconds callback.
  * @retval None
  */
void STM32RTC::detachSecondsInterrupt(void)
{
  detachSecondsIrqCallback();
}

#endif /* ONESECOND_IRQn */
// Kept for compatibility. Use STM32LowPower library.
void STM32RTC::standbyMode(void)
{

}

/*
 * Get Functions
 */

/**
  * @brief  get RTC subseconds.
  * @retval return the current subseconds from the RTC.
  */
uint32_t STM32RTC::getSubSeconds(void)
{
  syncTime();
  return _subSeconds;
}

/**
  * @brief  get RTC seconds.
  * @retval return the current seconds from the RTC.
  */
uint8_t STM32RTC::getSeconds(void)
{
  syncTime();
  return _seconds;
}

/**
  * @brief  get RTC minutes.
  * @retval return the current minutes from the RTC.
  */
uint8_t STM32RTC::getMinutes(void)
{
  syncTime();
  return _minutes;
}

/**
  * @brief  get RTC hours.
  * @param  format: optional (default: nullptr)
  *         pointer to the current hour period set in the RTC: AM or PM
  * @retval return the current hours from the RTC.
  */
uint8_t STM32RTC::getHours(AM_PM *period)
{
  syncTime();
  if (period != nullptr) {
    *period = _hoursPeriod;
  }
  return _hours;
}

/**
  * @brief  get RTC time.
  * @param  hours: pointer to the current hours
  * @param  minutes: pointer to the current minutes
  * @param  seconds: pointer to the current seconds
  * @param  subSeconds: pointer to the current subSeconds
  * @param  period: optional (default: nullptr)
  *         pointer to the current hour period set in the RTC: AM or PM
  * @retval none
  */
void STM32RTC::getTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds, uint32_t *subSeconds, AM_PM *period)
{
  syncTime();
  if (hours != nullptr) {
    *hours = _hours;
  }
  if (minutes != nullptr) {
    *minutes = _minutes;
  }
  if (seconds != nullptr) {
    *seconds = _seconds;
  }
  if (subSeconds != nullptr) {
    *subSeconds = _subSeconds;
  }
  if (period != nullptr) {
    *period = _hoursPeriod;
  }
}

/**
  * @brief  get RTC week day.
  * @retval return the current week day from the RTC.
  */
uint8_t STM32RTC::getWeekDay(void)
{
  syncDate();
  return _wday;
}

/**
  * @brief  get RTC day.
  * @retval return the current day from the RTC.
  */
uint8_t STM32RTC::getDay(void)
{
  syncDate();
  return _day;
}

/**
  * @brief  get RTC month.
  * @retval return the current month from the RTC.
  */
uint8_t STM32RTC::getMonth(void)
{
  syncDate();
  return _month;
}

/**
  * @brief  get RTC year.
  * @retval return the current year from the RTC.
  */
uint8_t STM32RTC::getYear(void)
{
  syncDate();
  return _year;
}

/**
  * @brief  get RTC time.
  * @param  weekDay: pointer to the current weekDay
  * @param  day: pointer to the current day
  * @param  month: pointer to the current month
  * @param  year: pointer to the current year
  * @retval none
  */
void STM32RTC::getDate(uint8_t *weekDay, uint8_t *day, uint8_t *month, uint8_t *year)
{
  syncDate();
  if (weekDay != nullptr) {
    *weekDay = _wday;
  }
  if (day != nullptr) {
    *day = _day;
  }
  if (month != nullptr) {
    *month = _month;
  }
  if (year != nullptr) {
    *year = _year;
  }
}

/**
  * @brief  get RTC alarm subsecond.
  * @retval return the current alarm subsecond.
  */
uint32_t STM32RTC::getAlarmSubSeconds(void)
{
  syncAlarmTime();
  return _alarmSubSeconds;
}

/**
  * @brief  get RTC alarm second.
  * @retval return the current alarm second.
  */
uint8_t STM32RTC::getAlarmSeconds(void)
{
  syncAlarmTime();
  return _alarmSeconds;
}

/**
  * @brief  get RTC alarm minute.
  * @retval return the current alarm minute.
  */
uint8_t STM32RTC::getAlarmMinutes(void)
{
  syncAlarmTime();
  return _alarmMinutes;
}

/**
  * @brief  get RTC alarm hour.
  * @param  format: optional (default: nullptr)
  *         pointer to the current hour format set in the RTC: AM or PM
  * @retval return the current alarm hour.
  */
uint8_t STM32RTC::getAlarmHours(AM_PM *period)
{
  syncAlarmTime();
  if (period != nullptr) {
    *period = _alarmPeriod;
  }
  return _alarmHours;
}

/**
  * @brief  get RTC alarm day.
  * @retval return the current alarm day.
  */
uint8_t STM32RTC::getAlarmDay(void)
{
  syncAlarmTime();
  return _alarmDay;
}

/**
  * @brief  get RTC alarm month.
  * @NOTE   This function is kept for compatibility but the STM32 RTC
  *         can't assign a month to an alarm. See board datasheet.
  * @retval always returns 0
  */
uint8_t STM32RTC::getAlarmMonth(void)
{
  return 0;
}

/**
  * @brief  get RTC alarm year.
  * @NOTE   This function is kept for compatibility but the STM32 RTC
  *         can't assign a year to an alarm. See board datasheet.
  * @retval always returns 0
  */
uint8_t STM32RTC::getAlarmYear(void)
{
  return 0;
}

/*
 * Set Functions
 */

/**
  * @brief  set RTC subseconds.
  * @param  subseconds: 0-999
  * @retval none
  */
void STM32RTC::setSubSeconds(uint32_t subSeconds)
{
  syncTime();
  if (subSeconds < 1000) {
    _subSeconds = subSeconds;
  }
  RTC_SetTime(_hours, _minutes, _seconds, _subSeconds, (_hoursPeriod == AM) ? HOUR_AM : HOUR_PM);
  _timeSet = true;
}

/**
  * @brief  set RTC seconds.
  * @param  seconds: 0-59
  * @retval none
  */
void STM32RTC::setSeconds(uint8_t seconds)
{
  syncTime();
  if (seconds < 60) {
    _seconds = seconds;
  }
  RTC_SetTime(_hours, _minutes, _seconds, _subSeconds, (_hoursPeriod == AM) ? HOUR_AM : HOUR_PM);
  _timeSet = true;
}

/**
  * @brief  set RTC minutes.
  * @param  minutes: 0-59
  * @retval none
  */
void STM32RTC::setMinutes(uint8_t minutes)
{
  syncTime();
  if (minutes < 60) {
    _minutes = minutes;
  }
  RTC_SetTime(_hours, _minutes, _seconds, _subSeconds, (_hoursPeriod == AM) ? HOUR_AM : HOUR_PM);
  _timeSet = true;
}

/**
  * @brief  set RTC hours.
  * @param  hours: 0-23
  * @param  period: hour format AM or PM (optional)
  * @retval none
  */
void STM32RTC::setHours(uint8_t hours, AM_PM period)
{
  syncTime();
  if (hours < 24) {
    _hours = hours;
  }
  if (_format == HOUR_12) {
    _hoursPeriod = period;
  }
  RTC_SetTime(_hours, _minutes, _seconds, _subSeconds, (_hoursPeriod == AM) ? HOUR_AM : HOUR_PM);
  _timeSet = true;
}

/**
  * @brief  set RTC time.
  * @param  hours: 0-23
  * @param  minutes: 0-59
  * @param  seconds: 0-59
  * @param  subSeconds: 0-999 (optional)
  * @param  period: hour format AM or PM (optional)
  * @retval none
  */
void STM32RTC::setTime(uint8_t hours, uint8_t minutes, uint8_t seconds, uint32_t subSeconds, AM_PM period)
{
  syncTime();
  if (subSeconds < 1000) {
    _subSeconds = subSeconds;
  }
  if (seconds < 60) {
    _seconds = seconds;
  }
  if (minutes < 60) {
    _minutes = minutes;
  }
  if (hours < 24) {
    _hours = hours;
  }
  if (_format == HOUR_12) {
    _hoursPeriod = period;
  }
  RTC_SetTime(_hours, _minutes, _seconds, _subSeconds, (_hoursPeriod == AM) ? HOUR_AM : HOUR_PM);
  _timeSet = true;
}

/**
  * @brief  set RTC week day.
  * @param  week day: 1-7 (Monday first)
  * @retval none
  */
void STM32RTC::setWeekDay(uint8_t weekDay)
{
  syncDate();
  if ((weekDay >= 1) && (weekDay <= 7)) {
    _wday = weekDay;
  }
  RTC_SetDate(_year, _month, _day, _wday);
  _timeSet = true;
}

/**
  * @brief  set RTC day.
  * @param  day: 1-31
  * @retval none
  */
void STM32RTC::setDay(uint8_t day)
{
  syncDate();
  if ((day >= 1) && (day <= 31)) {
    _day = day;
  }
  RTC_SetDate(_year, _month, _day, _wday);
  _timeSet = true;
}

/**
  * @brief  set RTC month.
  * @param  month: 1-12
  * @retval none
  */
void STM32RTC::setMonth(uint8_t month)
{
  syncDate();
  if ((month >= 1) && (month <= 12)) {
    _month = month;
  }
  RTC_SetDate(_year, _month, _day, _wday);
  _timeSet = true;
}

/**
  * @brief  set RTC year.
  * @param  year: 0-99
  * @retval none
  */
void STM32RTC::setYear(uint8_t year)
{
  syncDate();
  if (year < 100) {
    _year = year;
  }
  RTC_SetDate(_year, _month, _day, _wday);
  _timeSet = true;
}

/**
  * @brief  set RTC calendar.
  * @param  day: 1-31
  * @param  month: 1-12
  * @param  year: 0-99
  * @retval none
  */
void STM32RTC::setDate(uint8_t day, uint8_t month, uint8_t year)
{
  syncDate();
  if ((day >= 1) && (day <= 31)) {
    _day = day;
  }
  if ((month >= 1) && (month <= 12)) {
    _month = month;
  }
  if (year < 100) {
    _year = year;
  }
  RTC_SetDate(_year, _month, _day, _wday);
  _timeSet = true;
}

/**
  * @brief  set RTC calendar.
  * @param  weekDay: 1-7 (Monday first)
  * @param  day: 1-31
  * @param  month: 1-12
  * @param  year: 0-99
  * @retval none
  */
void STM32RTC::setDate(uint8_t weekDay, uint8_t day, uint8_t month, uint8_t year)
{
  syncDate();
  if ((weekDay >= 1) && (weekDay <= 7)) {
    _wday = weekDay;
  }
  if ((day >= 1) && (day <= 31)) {
    _day = day;
  }
  if ((month >= 1) && (month <= 12)) {
    _month = month;
  }
  if (year < 100) {
    _year = year;
  }
  RTC_SetDate(_year, _month, _day, _wday);
  _timeSet = true;
}

/**
  * @brief  set RTC alarm subseconds.
  * @param  subseconds: 0-999 (in ms)
  * @retval none
  */
void STM32RTC::setAlarmSubSeconds(uint32_t subSeconds)
{
  if (subSeconds < 1000) {
    _alarmSubSeconds = subSeconds;
  }
}

/**
  * @brief  set RTC alarm second.
  * @param  seconds: 0-59
  * @retval none
  */
void STM32RTC::setAlarmSeconds(uint8_t seconds)
{
  if (seconds < 60) {
    _alarmSeconds = seconds;
  }
}

/**
  * @brief  set RTC alarm minute.
  * @param  minutes: 0-59
  * @retval none
  */
void STM32RTC::setAlarmMinutes(uint8_t minutes)
{
  if (minutes < 60) {
    _alarmMinutes = minutes;
  }
}

/**
  * @brief  set RTC alarm hour.
  * @param  hour: 0-23 or 0-12
  * @param  period: hour format AM or PM (optional)
  * @retval none
  */
void STM32RTC::setAlarmHours(uint8_t hours, AM_PM period)
{
  if (hours < 24) {
    _alarmHours = hours;
  }
  if (_format == HOUR_12) {
    _alarmPeriod = period;
  }
}

/**
  * @brief  set RTC alarm time.
  * @param  hours: 0-23
  * @param  minutes: 0-59
  * @param  seconds: 0-59
  * @param  subSeconds: 0-999 (optional)
  * @param  period: hour format AM or PM (optional)
  * @retval none
  */
void STM32RTC::setAlarmTime(uint8_t hours, uint8_t minutes, uint8_t seconds, uint32_t subSeconds, AM_PM period)
{
  setAlarmHours(hours, period);
  setAlarmMinutes(minutes);
  setAlarmSeconds(seconds);
  setAlarmSubSeconds(subSeconds);
}

/**
  * @brief  set RTC alarm day.
  * @param  day: 1-31
  * @retval none
  */
void STM32RTC::setAlarmDay(uint8_t day)
{
  if ((day >= 1) && (day <= 31)) {
    _alarmDay = day;
  }
}

/**
  * @brief  set RTC alarm month.
  * @NOTE   This function is kept for compatibility but the STM32 RTC
  *         can't assign a month to an alarm. See board datasheet.
  * @param  month is ignored.
  */
void STM32RTC::setAlarmMonth(uint8_t month)
{
  UNUSED(month);
}

/**
  * @brief  set RTC alarm year.
  * @NOTE   This function is kept for compatibility but the STM32 RTC
  *         can't assign a year to an alarm. See board datasheet.
  * @param  year is ignored.
  */
void STM32RTC::setAlarmYear(uint8_t year)
{
  UNUSED(year);
}

/**
  * @brief  set RTC alarm date.
  * @NOTE   Parameters month and year are ignored because the STM32 RTC can't
  *         assign a month or year to an alarm. See board datasheet.
  * @param  day: 1-31
  * @param  month is ignored
  * @param  year is ignored
  */
void STM32RTC::setAlarmDate(uint8_t day, uint8_t month, uint8_t year)
{
  UNUSED(month);
  UNUSED(year);

  setAlarmDay(day);
}

/**
  * @brief  get epoch time
  * @param  subSeconds: optional pointer to where to store subseconds of the epoch in ms
  * @retval epoch time in seconds
  */
uint32_t STM32RTC::getEpoch(uint32_t *subSeconds)
{
  struct tm tm;

  syncDate();
  syncTime();

  tm.tm_isdst = -1;
  /*
   * mktime ignores the values supplied by the caller in the
   * tm_wday and tm_yday fields
   */
  tm.tm_yday = 0;
  tm.tm_wday = 0;
  tm.tm_year = _year + EPOCH_TIME_YEAR_OFF;
  tm.tm_mon = _month - 1;
  tm.tm_mday = _day;
  tm.tm_hour = _hours;
  tm.tm_min = _minutes;
  tm.tm_sec = _seconds;
  if (subSeconds != nullptr) {
    *subSeconds = _subSeconds;
  }

  return mktime(&tm);
}

/**
  * @brief  get epoch time since 1st January 2000, 00:00:00
  * @retval epoch time in seconds
  */
uint32_t STM32RTC::getY2kEpoch(void)
{
  return (getEpoch() - EPOCH_TIME_OFF);
}

/**
  * @brief  set RTC alarm from epoch time
  * @param  epoch time in seconds
  * @param  Alarm_Match match enum
  * @param  subSeconds subSeconds in ms
  */
void STM32RTC::setAlarmEpoch(uint32_t ts, Alarm_Match match, uint32_t subSeconds)
{
  if (ts < EPOCH_TIME_OFF) {
    ts = EPOCH_TIME_OFF;
  }

  time_t t = ts;
  struct tm *tmp = gmtime(&t);

  setAlarmDay(tmp->tm_mday);
  setAlarmHours(tmp->tm_hour);
  setAlarmMinutes(tmp->tm_min);
  setAlarmSeconds(tmp->tm_sec);
  setAlarmSubSeconds(subSeconds);
  enableAlarm(match);
}

/**
  * @brief  set RTC time from epoch time
  * @param  epoch time in seconds
  * @param  subSeconds subSeconds in ms
  */
void STM32RTC::setEpoch(uint32_t ts, uint32_t subSeconds)
{
  if (ts < EPOCH_TIME_OFF) {
    ts = EPOCH_TIME_OFF;
  }

  time_t t = ts;
  struct tm *tmp = gmtime(&t);

  _year = tmp->tm_year - EPOCH_TIME_YEAR_OFF;
  _month = tmp->tm_mon + 1;
  _day = tmp->tm_mday;
  if (tmp->tm_wday == 0) {
    _wday = RTC_WEEKDAY_SUNDAY;
  } else {
    _wday = tmp->tm_wday;
  }
  _hours = tmp->tm_hour;
  _minutes = tmp->tm_min;
  _seconds = tmp->tm_sec;
  _subSeconds = subSeconds;

  RTC_SetDate(_year, _month, _day, _wday);
  RTC_SetTime(_hours, _minutes, _seconds, _subSeconds, (_hoursPeriod == AM) ? HOUR_AM : HOUR_PM);
  _timeSet = true;
}

/**
  * @brief  set RTC time from epoch time since 1st January 2000, 00:00:00
  * @param  epoch time in seconds
  */
void STM32RTC::setY2kEpoch(uint32_t ts)
{
  setEpoch(ts + EPOCH_TIME_OFF);
}

/**
  * @brief  configure RTC source clock for low power
  * @param  none
  */
void STM32RTC::configForLowPower(Source_Clock source)
{
#if defined(HAL_PWR_MODULE_ENABLED)
#ifdef __HAL_RCC_RTCAPB_CLKAM_ENABLE
  __HAL_RCC_RTCAPB_CLKAM_ENABLE();
#endif

  begin();

  if (_clockSource != source) {
    // Save current config
    AM_PM period, alarmPeriod = _alarmPeriod;
    uint32_t subSeconds;
    uint8_t seconds, minutes, hours, weekDay, day, month, years;
    uint8_t alarmSeconds, alarmMinutes, alarmHours, alarmDay;
    Alarm_Match alarmMatch = _alarmMatch;

    alarmDay = _alarmDay;
    alarmHours = _alarmHours;
    alarmMinutes = _alarmMinutes;
    alarmSeconds = _alarmSeconds;

    getDate(&weekDay, &day, &month, &years);
    getTime(&seconds, &minutes, &hours, &subSeconds, &period);

    end();
    _clockSource = source;
    // Enable RTC
    begin(_format);
    // Restore config
    setTime(seconds, minutes, hours, subSeconds, period);
    setDate(weekDay, day, month, years);
    setAlarmTime(alarmHours, alarmMinutes, alarmSeconds, alarmPeriod);
    setAlarmDay(alarmDay);
    if (RTC_IsAlarmSet()) {
      enableAlarm(alarmMatch);
    }
  }

  if (!isTimeSet()) {
    // Set arbitrary time for Lowpower; if not already set
    setTime(12, 0, 0, 0, AM);
  }
#endif
}

/**
  * @brief  synchronise the time from the current RTC one
  * @param  none
  */
void STM32RTC::syncTime(void)
{
  hourAM_PM_t p = HOUR_AM;
  RTC_GetTime(&_hours, &_minutes, &_seconds, &_subSeconds, &p);
  _hoursPeriod = (p == HOUR_AM) ? AM : PM;
}

/**
  * @brief  synchronise the time from the current RTC one
  * @param  none
  */
void STM32RTC::syncDate(void)
{
  RTC_GetDate(&_year, &_month, &_day, &_wday);
}

/**
  * @brief  synchronise the alarm time from the current RTC one
  * @param  none
  */
void STM32RTC::syncAlarmTime(void)
{
  hourAM_PM_t p = HOUR_AM;
  uint8_t match;
  RTC_GetAlarm(&_alarmDay, &_alarmHours, &_alarmMinutes, &_alarmSeconds,
               &_alarmSubSeconds, &p, &match);
  _alarmPeriod = (p == HOUR_AM) ? AM : PM;
  switch (static_cast<Alarm_Match>(match)) {
    case MATCH_OFF:
    case MATCH_YYMMDDHHMMSS://kept for compatibility
    case MATCH_MMDDHHMMSS:  //kept for compatibility
    case MATCH_DHHMMSS:
    case MATCH_HHMMSS:
    case MATCH_MMSS:
    case MATCH_SS:
      _alarmMatch = static_cast<Alarm_Match>(match);
      break;
    default:
      _alarmMatch = MATCH_OFF;
      break;
  }
}
