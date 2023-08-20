#include "system_time.hpp"
#include <math.h>

namespace el1::system::time
{
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
}
