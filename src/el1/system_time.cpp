#include "system_time.hpp"
#include "error.hpp"
#include <math.h>
#include <limits>

namespace el1::system::time
{

	namespace
	{
		static constexpr s64_t ATTOS_PER_SECOND = 1000000000000000000LL;
		static constexpr s64_t SECONDS_PER_POSIX_DAY = 86400LL;	// POSIX/Unix time does not represent leap seconds.
		static constexpr s64_t UNIX_EPOCH_JDN = 2440588LL;
		static constexpr s64_t GREGORIAN_START_JDN = 2299161LL;

		s64_t FloorDiv(const s64_t numerator, const s64_t denominator) noexcept
		{
			s64_t quotient = numerator / denominator;
			if(numerator % denominator < 0)
				quotient--;
			return quotient;
		}

		ECalendarSystem CalendarSystemForDate(const s64_t year, const unsigned month, const unsigned day) noexcept
		{
			if(year > 1582)
				return ECalendarSystem::GREGORIAN;
			if(year < 1582)
				return ECalendarSystem::JULIAN;
			if(month > 10)
				return ECalendarSystem::GREGORIAN;
			if(month < 10)
				return ECalendarSystem::JULIAN;
			return day >= 15 ? ECalendarSystem::GREGORIAN : ECalendarSystem::JULIAN;
		}

		bool IsLeapYear(const s64_t year, const ECalendarSystem calendar_system) noexcept
		{
			if(calendar_system == ECalendarSystem::JULIAN)
				return year % 4 == 0;
			return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
		}

		u8_t DaysInMonth(const s64_t year, const u8_t month, const ECalendarSystem calendar_system) noexcept
		{
			static constexpr u8_t DAYS[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
			if(month == 2 && IsLeapYear(year, calendar_system))
				return 29;
			return DAYS[month - 1];
		}

		s64_t DateToJdn(const s64_t year, const u8_t month, const u8_t day, const ECalendarSystem calendar_system)
		{
			// Any date representable as TTime is within roughly +/-292 billion years.
			// Keeping this bound here also makes all intermediate 64-bit arithmetic safe.
			EL_ERROR(year < -300000000000LL || year > 300000000000LL, error::TInvalidArgumentException, "year", "date is outside the TTime timestamp range");

			const s64_t a = (14 - static_cast<s64_t>(month)) / 12;
			const s64_t y = year + 4800 - a;
			const s64_t m = static_cast<s64_t>(month) + 12 * a - 3;
			const s64_t common = static_cast<s64_t>(day) + (153 * m + 2) / 5 + 365 * y;

			if(calendar_system == ECalendarSystem::GREGORIAN)
				return common + FloorDiv(y, 4) - FloorDiv(y, 100) + FloorDiv(y, 400) - 32045;
			return common + FloorDiv(y, 4) - 32083;
		}
	}

	struct TCalendar::TFields
	{
		s64_t year;
		u8_t month;
		u8_t day;
		u8_t hour;
		u8_t minute;
		u8_t second;
		u64_t attoseconds;
		ECalendarSystem calendar_system;
	};

	void	TTime::Normalize	() noexcept
	{
		if(asec <= -1000000000000000000LL)
		{
			const s64_t f = asec / 1000000000000000000LL;
			sec += f;
			asec -= f * 1000000000000000000LL;
		}
		else if(asec >= 1000000000000000000LL)
		{
			const s64_t f = asec / 1000000000000000000LL;
			sec += f;
			asec -= f * 1000000000000000000LL;
		}

		if(asec < 0 && sec > 0)
		{
			sec--;
			asec += 1000000000000000000LL;
		}
		else if(asec > 0 && sec < 0)
		{
			sec++;
			asec -= 1000000000000000000LL;
		}
	}

	TTime&	TTime::operator+=	(const TTime op) noexcept
	{
		sec += op.sec;
		asec += op.asec;

		Normalize();

		return *this;
	}

	TTime&	TTime::operator-=	(const TTime op) noexcept
	{
		sec -= op.sec;
		asec -= op.asec;

		Normalize();

		return *this;
	}

	TTime	TTime::operator+	(const TTime op) const noexcept
	{
		return TTime(*this) += op;
	}

	TTime	TTime::operator-	(const TTime op) const noexcept
	{
		return TTime(*this) -= op;
	}

	TTime&	TTime::operator*=	(const double f) noexcept
	{
		this->sec *= f;
		this->asec *= f;
		Normalize();
		return *this;
	}

	TTime&	TTime::operator/=	(const double f) noexcept
	{
		this->asec += fmod(this->sec, f) * 1000000000000000000LL;
		this->asec /= f;
		this->sec /= f;
		Normalize();
		return *this;
	}

	TTime	TTime::operator*	(const double f) const noexcept
	{
		return TTime(*this) *= f;
	}

	TTime	TTime::operator/	(const double f) const noexcept
	{
		return TTime(*this) /= f;
	}

	TTime&	TTime::operator*=	(const s64_t f) noexcept
	{
		this->sec *= f;
		this->asec *= f;
		Normalize();
		return *this;
	}

	TTime&	TTime::operator/=	(const s64_t f) noexcept
	{
		this->asec += (this->sec % f) * 1000000000000000000LL;
		this->asec /= f;
		this->sec /= f;
		Normalize();
		return *this;
	}

	TTime	TTime::operator*	(const s64_t f) const noexcept
	{
		return TTime(*this) *= f;
	}

	TTime	TTime::operator/	(const s64_t f) const noexcept
	{
		return TTime(*this) /= f;
	}

	bool	TTime::operator>	(const TTime op) const noexcept
	{
		if(sec > op.sec)
			return true;
		if(sec < op.sec)
			return false;
		return asec > op.asec;
	}

	bool	TTime::operator<	(const TTime op) const noexcept
	{
		if(sec < op.sec)
			return true;
		if(sec > op.sec)
			return false;
		return asec < op.asec;
	}

	bool	TTime::operator>=	(const TTime op) const noexcept
	{
		if(sec > op.sec)
			return true;
		if(sec < op.sec)
			return false;
		return asec >= op.asec;
	}

	bool	TTime::operator<=	(const TTime op) const noexcept
	{
		if(sec < op.sec)
			return true;
		if(sec > op.sec)
			return false;
		return asec <= op.asec;
	}

	bool	TTime::operator==	(const TTime op) const noexcept
	{
		return sec == op.sec && asec == op.asec;
	}

	bool	TTime::operator!=	(const TTime op) const noexcept
	{
		return sec != op.sec || asec != op.asec;
	}

	s64_t		TTime::ConvertToI	(EUnit cunit_) const noexcept
	{
		const int cunit = (int)cunit_;
		if(cunit < 0)
		{
			s64_t mul = 1LL;
			s64_t div = 1000000000000000000LL;

			for(int i = 0; i > cunit; i--)
			{
				mul *= 1000LL;
				div /= 1000LL;
			}

			return sec * mul + asec / div;
		}
		else
			return sec / cunit;
	}

	double	TTime::ConvertToF	(EUnit cunit_) const noexcept
	{
		const int cunit = (int)cunit_;
		if(cunit < 0)
		{
			double mul = 1.0;
			double div = 1.0/1000000000000000000.0;

			for(int i = 0; i > cunit; i--)
			{
				mul *= 1000.0;
				div *= 1000.0;
			}

			return (double)sec * mul + (double)asec * div;
		}
		else
			return ((double)sec + (double)asec / 1000000000000000000.0) / (double)cunit;
	}

	TTime	TTime::ConvertFrom	(EUnit cunit_, s64_t value) noexcept
	{
		const int cunit = (int)cunit_;
		if(cunit < 0)
		{
			s64_t div = 1LL;
			s64_t mul = 1000000000000000000LL;

			for(int i = 0; i > cunit; i--)
			{
				div *= 1000LL;
				mul /= 1000LL;
			}

			const s64_t sec = value / div;
			value -= sec * div;

			return TTime((s64_t)sec, (s64_t)(value * mul));
		}
		else
			return TTime(value * cunit, (s64_t)0);
	}

	TTime	TTime::ConvertFrom	(EUnit cunit_, double value) noexcept
	{
		const int cunit = (int)cunit_;
		if(cunit < 0)
		{
			double div = 1.0;
			double mul = 1.0/1000000000000000000.0;

			for(int i = 0; i > cunit; i--)
			{
				div *= 1000.0;
				mul *= 1000.0;
			}

			s64_t sec = (s64_t)(value / div);
			value -= sec * div;

			return TTime(sec, (s64_t)(value / mul));
		}
		else
		{
			value *= (double)cunit;

			s64_t sec = (s64_t)value;
			value -= sec;

			return TTime(sec, (s64_t)(value * 1000000000000000000.0));
		}
	}

	s64_t	TTime::ConvertToI	(s64_t  tps) const noexcept
	{
		return this->sec * tps + this->asec / (1000000000000000000LL / tps);
	}

	double	TTime::ConvertToF	(double tps) const noexcept
	{
		return this->sec * tps + this->asec / (1000000000000000000LL / tps);
	}

	TTime	TTime::ConvertFrom	(s64_t  tps, s64_t value) noexcept
	{
		const s64_t sec = value / tps;
		value -= sec * tps;	// compute fractional part
		value *= 1000000000000000000ULL / tps;	// ticks per attosecond
		return TTime(sec, value);
	}

	TTime	TTime::ConvertFrom	(double tps, double value) noexcept
	{
		return TTime(value / tps);
	}

	TTime::TTime(s64_t seconds, s64_t attoseconds) noexcept : sec(seconds), asec(attoseconds)
	{
		Normalize();
	}

	TTime::TTime(struct timespec ts) noexcept : sec((s64_t)ts.tv_sec), asec((s64_t)ts.tv_nsec * (s64_t)1000000000LL)
	{
		Normalize();
	}

	TTime::TTime(struct timeval tv) noexcept : sec((s64_t)tv.tv_sec), asec((s64_t)tv.tv_usec * (s64_t)1000000000000LL)
	{
		Normalize();
	}

	TCalendar::TCalendar(
		const s64_t year,
		const unsigned month,
		const unsigned day,
		const unsigned hour,
		const unsigned minute,
		const unsigned second,
		const u64_t attoseconds
	) :
		year(year),
		attoseconds(attoseconds),
		month(static_cast<u8_t>(month)),
		day(static_cast<u8_t>(day)),
		hour(static_cast<u8_t>(hour)),
		minute(static_cast<u8_t>(minute)),
		second(static_cast<u8_t>(second)),
		calendar_system(CalendarSystemForDate(year, month, day))
	{
		EL_ERROR(month < 1 || month > 12, error::TInvalidArgumentException, "month", "month must be in the range 1..12");
		EL_ERROR(day < 1 || day > DaysInMonth(year, static_cast<u8_t>(month), calendar_system), error::TInvalidArgumentException, "day", "day is invalid for the selected month and calendar");
		EL_ERROR(year == 1582 && month == 10 && day >= 5 && day <= 14, error::TInvalidArgumentException, "day", "1582-10-05 through 1582-10-14 were skipped by the Gregorian calendar reform");
		EL_ERROR(hour > 23, error::TInvalidArgumentException, "hour", "hour must be in the range 0..23");
		EL_ERROR(minute > 59, error::TInvalidArgumentException, "minute", "minute must be in the range 0..59");
		EL_ERROR(second > 59, error::TInvalidArgumentException, "second", "second must be in the range 0..59");
		EL_ERROR(attoseconds >= static_cast<u64_t>(ATTOS_PER_SECOND), error::TInvalidArgumentException, "attoseconds", "attoseconds must be less than one second");
	}

	TCalendar::TCalendar(const TFields& fields) :
		year(fields.year),
		attoseconds(fields.attoseconds),
		month(fields.month),
		day(fields.day),
		hour(fields.hour),
		minute(fields.minute),
		second(fields.second),
		calendar_system(fields.calendar_system)
	{
	}

	TCalendar::TFields TCalendar::Decode(const TTime timestamp)
	{
		s64_t days = timestamp.Seconds() / SECONDS_PER_POSIX_DAY;
		s64_t second_of_day = timestamp.Seconds() % SECONDS_PER_POSIX_DAY;
		if(second_of_day < 0)
		{
			days--;
			second_of_day += SECONDS_PER_POSIX_DAY;
		}

		s64_t attoseconds = timestamp.Attoseconds();
		if(attoseconds < 0)
		{
			attoseconds += ATTOS_PER_SECOND;
			if(second_of_day > 0)
				second_of_day--;
			else
			{
				days--;
				second_of_day = SECONDS_PER_POSIX_DAY - 1;
			}
		}

		const s64_t jdn = UNIX_EPOCH_JDN + days;
		TFields fields = {};
		fields.hour = static_cast<u8_t>(second_of_day / 3600);
		second_of_day %= 3600;
		fields.minute = static_cast<u8_t>(second_of_day / 60);
		fields.second = static_cast<u8_t>(second_of_day % 60);
		fields.attoseconds = static_cast<u64_t>(attoseconds);

		if(jdn >= GREGORIAN_START_JDN)
		{
			fields.calendar_system = ECalendarSystem::GREGORIAN;
			const s64_t a = jdn + 32044;
			const s64_t b = FloorDiv(4 * a + 3, 146097);
			const s64_t c = a - FloorDiv(146097 * b, 4);
			const s64_t d = FloorDiv(4 * c + 3, 1461);
			const s64_t e = c - FloorDiv(1461 * d, 4);
			const s64_t m = FloorDiv(5 * e + 2, 153);
			fields.day = static_cast<u8_t>(e - FloorDiv(153 * m + 2, 5) + 1);
			fields.month = static_cast<u8_t>(m + 3 - 12 * FloorDiv(m, 10));
			fields.year = 100 * b + d - 4800 + FloorDiv(m, 10);
		}
		else
		{
			fields.calendar_system = ECalendarSystem::JULIAN;
			const s64_t c = jdn + 32082;
			const s64_t d = FloorDiv(4 * c + 3, 1461);
			const s64_t e = c - FloorDiv(1461 * d, 4);
			const s64_t m = FloorDiv(5 * e + 2, 153);
			fields.day = static_cast<u8_t>(e - FloorDiv(153 * m + 2, 5) + 1);
			fields.month = static_cast<u8_t>(m + 3 - 12 * FloorDiv(m, 10));
			fields.year = d - 4800 + FloorDiv(m, 10);
		}

		return fields;
	}

	TCalendar::TCalendar(const TTime timestamp) : TCalendar(Decode(timestamp))
	{
	}

	TTime TCalendar::ConvertToTime() const
	{
		const s64_t jdn = DateToJdn(year, month, day, calendar_system);
		const s64_t days = jdn - UNIX_EPOCH_JDN;
		const s64_t second_of_day = static_cast<s64_t>(hour) * 3600 + static_cast<s64_t>(minute) * 60 + second;

		s64_t seconds;
		if(days >= 0)
		{
			const u64_t magnitude = static_cast<u64_t>(days) * static_cast<u64_t>(SECONDS_PER_POSIX_DAY) + static_cast<u64_t>(second_of_day);
			EL_ERROR(magnitude > static_cast<u64_t>(std::numeric_limits<s64_t>::max()), error::TInvalidArgumentException, "year", "date is outside the TTime timestamp range");
			seconds = static_cast<s64_t>(magnitude);
		}
		else
		{
			const u64_t magnitude = static_cast<u64_t>(-days) * static_cast<u64_t>(SECONDS_PER_POSIX_DAY) - static_cast<u64_t>(second_of_day);
			const u64_t max_negative_magnitude = static_cast<u64_t>(std::numeric_limits<s64_t>::max()) + 1U;
			if(magnitude == max_negative_magnitude + 1U && attoseconds > 0)
				return TTime(std::numeric_limits<s64_t>::min(), static_cast<s64_t>(attoseconds) - ATTOS_PER_SECOND);
			EL_ERROR(magnitude > max_negative_magnitude, error::TInvalidArgumentException, "year", "date is outside the TTime timestamp range");
			seconds = magnitude == max_negative_magnitude ? std::numeric_limits<s64_t>::min() : -static_cast<s64_t>(magnitude);
		}

		return TTime(seconds, static_cast<s64_t>(attoseconds));
	}

	TTime::TTime(const TCalendar& calendar) : TTime(calendar.ConvertToTime())
	{
	}

	TCalendar TTime::ConvertToCalendar() const
	{
		return TCalendar(*this);
	}

}
