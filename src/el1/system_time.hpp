#pragma once

#include "def.hpp"
#include "io_types.hpp"

#ifdef EL_OS_CLASS_WINDOWS
	#include <minwinbase.h>
#endif

#ifdef EL_OS_CLASS_POSIX
	#include <time.h>
	#include <sys/time.h>
#endif

namespace el1::system::time
{
	using namespace io::types;

	enum class EUnit : s32_t
	{
		ATTOSECONDS  = -6,	// 1.000.000.000.000.000.000
		FEMTOSECONDS = -5,	// 1.000.000.000.000.000
		PICOSECONDS  = -4,	// 1.000.000.000.000
		NANOSECONDS  = -3,	// 1.000.000.000
		MICROSECONDS = -2,	// 1.000.000
		MILLISECONDS = -1,	// 1.000
		SECONDS =     1,
		MINUTES =    60,
		HOURS   =  3600,
		DAYS    = 86400,
		//	no month, year, etc. because they are not constant
	};

	enum class EClock
	{
		REALTIME,		//	System-wide real-time clock (based on unixtime - *NOT* MONOTONIC !!!).
		MONOTONIC,		//	Clock that cannot be set and represents monotonic time since some unspecified (and possibly changing) starting point not subject to NTP/leap adjustments.
		PROCESS,		//	Process time is defined as the amount of CPU time consumed by a process.
		PROCESS_USER,	//	as above but only the portion spent in user-mode
		PROCESS_SYS,	//	as above but only the portion spent in kernel-mode
		THREAD,			//	Thread time is defined as the amount of CPU time consumed by a (/ the calling) thread.
		THREAD_USER,	//	as above but only the portion spent in user-mode
		THREAD_SYS,		//	as above but only the portion spent in kernel-mode
	};

	class TTime
	{
		private:
			void	Normalize	() noexcept;

		protected:
			s64_t	sec;	//	seconds since 1970-01-01 00:00 +0000 (UTC)
			s64_t	asec;	//	atto-seconds (10^-18) of current second

		public:
			inline	s64_t	Seconds		() const noexcept EL_GETTER { return sec; }
			inline	s64_t	Attoseconds	() const noexcept EL_GETTER { return asec; }

			TTime&	operator+=	(const TTime op) noexcept;
			TTime&	operator-=	(const TTime op) noexcept;
			TTime	operator+	(const TTime op) const noexcept EL_GETTER;
			TTime	operator-	(const TTime op) const noexcept EL_GETTER;

			TTime&	operator*=	(const double f) noexcept;
			TTime&	operator/=	(const double f) noexcept;
			TTime	operator*	(const double f) const noexcept EL_GETTER;
			TTime	operator/	(const double f) const noexcept EL_GETTER;

			TTime&	operator*=	(const s64_t f) noexcept;
			TTime&	operator/=	(const s64_t f) noexcept;
			TTime	operator*	(const s64_t f) const noexcept EL_GETTER;
			TTime	operator/	(const s64_t f) const noexcept EL_GETTER;

			bool	operator>	(const TTime op) const noexcept EL_GETTER;
			bool	operator<	(const TTime op) const noexcept EL_GETTER;
			bool	operator>=	(const TTime op) const noexcept EL_GETTER;
			bool	operator<=	(const TTime op) const noexcept EL_GETTER;
			bool	operator==	(const TTime op) const noexcept EL_GETTER;
			bool	operator!=	(const TTime op) const noexcept EL_GETTER;

			s64_t			ConvertToI	(const EUnit unit) const noexcept EL_GETTER;
			double			ConvertToF	(const EUnit unit) const noexcept EL_GETTER;
			static TTime	ConvertFrom	(const EUnit unit, s64_t value)  noexcept EL_GETTER;
			static TTime	ConvertFrom	(const EUnit unit, double value) noexcept EL_GETTER;

			s64_t			ConvertToI	(const s64_t  tps) const noexcept EL_GETTER;
			double			ConvertToF	(const double tps) const noexcept EL_GETTER;
			static TTime	ConvertFrom	(const s64_t  tps, s64_t value)  noexcept EL_GETTER;
			static TTime	ConvertFrom	(const double tps, double value) noexcept EL_GETTER;

			static	TTime	Now			(const EClock clock = EClock::REALTIME);

			inline TTime() noexcept : sec(0), asec(0) {}
			constexpr TTime(const int seconds) noexcept : sec(seconds), asec(0) {}
			constexpr TTime(const unsigned seconds) noexcept : sec(seconds), asec(0) {}
			constexpr TTime(double seconds) noexcept
			{
				this->sec = (s64_t)seconds;
				seconds -= this->sec;
				this->asec = (s64_t)(seconds * 1000000000000000000.0);
			}

			TTime(const s64_t seconds, const s64_t attoseconds) noexcept;

			#ifdef EL_OS_CLASS_POSIX
				operator timespec() const noexcept;
				operator timeval() const noexcept;
				TTime(struct timespec ts) noexcept;
				TTime(struct timeval tv) noexcept;
			#endif

			#ifdef EL_OS_CLASS_WINDOWS
				operator FILETIME() const noexcept;
				TTime(const FILETIME ts) noexcept;
			#endif
	};
}
