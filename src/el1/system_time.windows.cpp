#include "def.hpp"
#ifdef EL_OS_CLASS_WINDOWS

#include "system_time.hpp"
#include "error.hpp"
#include <windows.h>

using namespace cl3::system::error;

namespace	cl3
{
	namespace	system
	{
		namespace	time
		{
			static const u64 offset_unixtime = (1969ULL * 365ULL + 1969ULL / 4ULL - 10ULL - 3ULL) * (24ULL * 60ULL * 60ULL) - (584400LL - 10LL) * 86400LL;

			static LARGE_INTEGER hpc_start;
			static LARGE_INTEGER hpc_freq;
			static LARGE_INTEGER hpc_multi;

			namespace
			{
				struct	T
				{
					T()
					{
						//	called when the lib is loaded by the application (even before main() is executed, except if the lib is dynamically loaded)
						SYSERR(QueryPerformanceFrequency(&hpc_freq) == 0);
						SYSERR(QueryPerformanceCounter(&hpc_start) == 0);
						hpc_multi.QuadPart = 1000000000000000000LL / hpc_freq.QuadPart;
					}

					~T()
					{
						//	called when the lib is unloaded (after main(), except is the lib is dynamically loaded)
					}
				};
				const static T __init_trigger_object;	//	this object triggers the call to the init routine when libcl3 is loaded or unloaded
			}

			TTime	TTime::Now			(EClock clock)
			{
				switch(clock)
				{
					case EClock::REALTIME:
					{
						SYSTEMTIME st;
						FILETIME ft;	//	 100-nanosecond intervals since January 1, 1601 (UTC).
						ULARGE_INTEGER li;

						GetSystemTime(&st);
						SystemTimeToFileTime(&st, &ft);

						li.LowPart = ft.dwLowDateTime;
						li.HighPart = ft.dwHighDateTime;

						//	584400 days from 0001-01-01 to 1601-01-01
						//	   -11 days (thanks to Gregor XIII)
						//	*86400 seconds per day
						//const s64_t add = offset_year1;

						s64_t sec = li.QuadPart / 10000000LL;
						li.QuadPart -= sec * 10000000LL;

						return TTime(sec - offset_unixtime, li.QuadPart * 100000000000LL);
					}

					case EClock::MONOTONIC:
					{
						LARGE_INTEGER hpc_now;
						SYSERR(QueryPerformanceCounter(&hpc_now) == 0);

						LARGE_INTEGER hpc_delta;
						hpc_delta.QuadPart = (hpc_now.QuadPart - hpc_start.QuadPart);

						return TTime(hpc_delta.QuadPart / hpc_freq.QuadPart, (hpc_delta.QuadPart % hpc_freq.QuadPart) * hpc_multi.QuadPart);
					}



					case EClock::PROCESS:
					{
						FILETIME results[4];
						SYSERR(GetProcessTimes(GetCurrentProcess(),results, results+1, results+2, results+3) == 0);
						ULARGE_INTEGER kernel_time;
						ULARGE_INTEGER user_time;
						ULARGE_INTEGER total_time;
						kernel_time.LowPart = results[2].dwLowDateTime;
						kernel_time.HighPart = results[2].dwHighDateTime;
						user_time.LowPart = results[3].dwLowDateTime;
						user_time.HighPart = results[3].dwHighDateTime;

						total_time.QuadPart = kernel_time.QuadPart + user_time.QuadPart;

						s64_t sec = total_time.QuadPart / 10000000LL;
						total_time.QuadPart -= sec * 10000000LL;


						return TTime(sec, total_time.QuadPart * 100000000000LL);
					}
					case EClock::PROCESS_USER:
					{
						FILETIME results[4];
						SYSERR(GetProcessTimes(GetCurrentProcess(),results, results+1, results+2, results+3) == 0);
						ULARGE_INTEGER user_time;
						user_time.LowPart = results[3].dwLowDateTime;
						user_time.HighPart = results[3].dwHighDateTime;

						s64_t sec = user_time.QuadPart / 10000000LL;
						user_time.QuadPart -= sec * 10000000LL;

						return TTime(sec, user_time.QuadPart * 100000000000LL);
					}
					case EClock::PROCESS_SYS:
					{
						FILETIME results[4];
						SYSERR(GetProcessTimes(GetCurrentProcess(),results, results+1, results+2, results+3) == 0);
						ULARGE_INTEGER kernel_time;
						kernel_time.LowPart = results[2].dwLowDateTime;
						kernel_time.HighPart = results[2].dwHighDateTime;

						s64_t sec = kernel_time.QuadPart / 10000000LL;
						kernel_time.QuadPart -= sec * 10000000LL;

						return TTime(sec, kernel_time.QuadPart * 100000000000LL);
					}


					case EClock::THREAD:
					{
						FILETIME results[4];
						SYSERR(GetThreadTimes(GetCurrentThread(),results, results+1, results+2, results+3) == 0);
						ULARGE_INTEGER kernel_time;
						ULARGE_INTEGER user_time;
						ULARGE_INTEGER total_time;
						kernel_time.LowPart = results[2].dwLowDateTime;
						kernel_time.HighPart = results[2].dwHighDateTime;
						user_time.LowPart = results[3].dwLowDateTime;
						user_time.HighPart = results[3].dwHighDateTime;

						total_time.QuadPart = kernel_time.QuadPart + user_time.QuadPart;

						s64_t sec = total_time.QuadPart / 10000000LL;
						total_time.QuadPart -= sec * 10000000LL;


						return TTime(sec, total_time.QuadPart * 100000000000LL);
					}
					case EClock::THREAD_USER:
					{
						FILETIME results[4];
						SYSERR(GetThreadTimes(GetCurrentThread(),results, results+1, results+2, results+3) == 0);
						ULARGE_INTEGER user_time;
						user_time.LowPart = results[3].dwLowDateTime;
						user_time.HighPart = results[3].dwHighDateTime;

						s64_t sec = user_time.QuadPart / 10000000LL;
						user_time.QuadPart -= sec * 10000000LL;

						return TTime(sec, user_time.QuadPart * 100000000000LL);
					}
					case EClock::THREAD_SYS:
					{
						FILETIME results[4];
						SYSERR(GetThreadTimes(GetCurrentThread(),results, results+1, results+2, results+3) == 0);
						ULARGE_INTEGER kernel_time;
						kernel_time.LowPart = results[2].dwLowDateTime;
						kernel_time.HighPart = results[2].dwHighDateTime;

						s64_t sec = kernel_time.QuadPart / 10000000LL;
						kernel_time.QuadPart -= sec * 10000000LL;

						return TTime(sec, kernel_time.QuadPart * 100000000000LL);
					}

					default:
						ERROR_S(TArgumentException, "clock", clock);
				}
			}
		}
	}
}

#endif
