#include <el1/el1.cpp>
#include <time.h>
#include <sys/shm.h>
// 24 => PON
// 23 => SIGNAL
// distance = 74000

using namespace el1::io::types;
using namespace el1::system::time;

static const key_t CHRONY_SHM_KEY = 0x4e545030;
struct chrony_shm_t
{
	int mode;   /* 0 - if valid set
				*       use values,
				*       clear valid
				* 1 - if valid set
				*       if count before and after read of values is equal,
				*         use values
				*       clear valid
				*/
	volatile int count;
#if defined(_TIME_BITS) && 64 == _TIME_BITS
	/* libc default time_t is 32-bits, but we are using 64-bits.
	* glibc 2.34 and later.
	* Lower 31 bits, no sign bit, are here. */
	unsigned clockTimeStampSec;
#else
	time_t clockTimeStampSec;
#endif
	int clockTimeStampUSec;
#if defined(_TIME_BITS) && 64 == _TIME_BITS
	/* libc default time_t is 32-bits, but we are using 64-bits.
	* glibc 2.34 and later.
	* Lower 31 bits, no sign bit, are here. */
	unsigned receiveTimeStampSec;
#else
	time_t receiveTimeStampSec;
#endif
	int receiveTimeStampUSec;
	int leap;                   // not leapsecond offset, a notification code
	int precision;              // log(2) of source jitter
	int nsamples;               // not used
	volatile int valid;
	unsigned clockTimeStampNSec;     // Unsigned ns timestamps
	unsigned receiveTimeStampNSec;   // Unsigned ns timestamps
	/* Change previous dummy[0,1] to hold top bits.
	* Zero until 2038.  */
	unsigned top_clockTimeStampSec;
	unsigned top_receiveTimeStampSec;
	int dummy[6];
};

static chrony_shm_t* chrony_shm = nullptr;
static TTime ts_dcf77 = TTime::Now(EClock::REALTIME);
static TTime ts_systime = 0;
static bool leap_announce = false;

static void AttachSharedMemory(const key_t unit)
{
	const int id = EL_SYSERR(shmget(CHRONY_SHM_KEY + unit, sizeof(chrony_shm_t), 0));
	chrony_shm = (chrony_shm_t*)EL_SYSERR(shmat(id, nullptr, 0));
	EL_ERROR(chrony_shm == nullptr, TLogicException);
	chrony_shm->valid = 0;
	__sync_synchronize();
	chrony_shm->mode = 1;
	chrony_shm->leap = 3;
	chrony_shm->precision = -20;
	chrony_shm->nsamples = 1;
	if(el1::dev::gpio::dcf77::DEBUG)
		printf("attached shm @ %p, count was %d\n", chrony_shm, chrony_shm->count);
	chrony_shm->count = 0;
}

static void UpdateShm()
{
	__sync_synchronize();
	chrony_shm->valid = 0;
	chrony_shm->count++;
	__sync_synchronize();
	chrony_shm->clockTimeStampSec    = ts_dcf77.Seconds();
	chrony_shm->clockTimeStampUSec   = ts_dcf77.Attoseconds() / 1000000000000LL;
	chrony_shm->clockTimeStampNSec   = ts_dcf77.Attoseconds() / 1000000000LL;
	chrony_shm->receiveTimeStampSec  = ts_systime.Seconds();
	chrony_shm->receiveTimeStampUSec = ts_systime.Attoseconds() / 1000000000000LL;
	chrony_shm->receiveTimeStampNSec = ts_systime.Attoseconds() / 1000000000LL;
	chrony_shm->leap = leap_announce ? 1 : 0;
	__sync_synchronize();
	chrony_shm->count++;
	const int count = chrony_shm->count;
	__sync_synchronize();
	chrony_shm->valid = 1;
	__sync_synchronize();

	if(el1::dev::gpio::dcf77::DEBUG)
		printf("updated shm (count was %d)\n", count);
}

static void OnUpdate(const ::tm& t, u64_t ns, bool _leap_announce)
{
	ts_systime = TTime::Now(EClock::REALTIME);
	leap_announce = _leap_announce;

	if(chrony_shm)
	{
		::tm t2 = t;
		ts_dcf77 = TTime(mktime(&t2), ns * 1000000000LL);
		UpdateShm();
	}

	const TTime delta = ts_dcf77 - ts_systime;
	char format[64];
	char buffer[128];

	putchar('\n');

	// print systime with nanoseconds
	snprintf(format, sizeof(format), "CLOCK: %%a, %%d %%b %%Y %%T.%09llu %%z", ts_systime.Attoseconds() / 1000000000LL);
	strftime(buffer, sizeof(buffer), format, &t);
	puts(buffer);

	// print DCF77-time with nanoseconds and delta to systime
	snprintf(format, sizeof(format), "DCF77: %%a, %%d %%b %%Y %%T.%09llu %%z, ΔT=%fms", ns, delta.ConvertToF(EUnit::MILLISECONDS));
	strftime(buffer, sizeof(buffer), format, &t);
	puts(buffer);
}

static void OnTick(const TTime&, int status)
{
	switch(status)
	{
		case 0:
		case 1:
			putchar('.');
			break;
		case -1:
			putchar('S');
			break;
		case -2:
			putchar('T');
			break;
		case -3:
			putchar('G');
			break;
		default:
			putchar('?');
			break;
	}
	fflush(stdout);
}

int main(int argc, char* argv[])
{
	using namespace el1::error;
	using namespace el1::dev::gpio::native;
	using namespace el1::dev::gpio::dcf77;
	using namespace el1::system::cmdline;
	using namespace el1::system::task;

	try
	{
		s64_t idx_signal;
		s64_t idx_chrony_shm = -1;
		double distance = 0;

		ParseCmdlineArguments(argc, argv,
			TFlagArgument(&DEBUG, 'd', "debug", "", "Enable debug output"),
			TIntegerArgument(&idx_signal, 'g', "gpio", "", false, false, "Configures the GPIO pin# used for DCF77 signaling"),
			TFloatArgument(&distance, 'm', "distance", "", true, false, "Sets the distance in meters from the DCF77 sending station in Mainhausen Germany. This is used to compute the signal runtime."),
			TIntegerArgument(&idx_chrony_shm, 's', "shm-index", "", true, false, "If set to a positive value it will write the received timestamp to the specified NTP shared-memory location - see gpsd and chronyd manpages for details")
		);

		if(idx_chrony_shm >= 0)
			AttachSharedMemory(idx_chrony_shm);
		TNativeGpioController& gpio_ctrl = *TNativeGpioController::Instance();
		TDCF77 dcf77(gpio_ctrl.ClaimPin(idx_signal), distance);
		dcf77.OnUpdate() += OnUpdate;
		dcf77.OnTick() += OnTick;
		TFiber::Self()->Stop();

		return 0;
	}
	catch(shutdown_t)
	{
		fprintf(stderr, "\nBYE!\n");
		return 0;
	}
	catch(const IException& e)
	{
		e.Print("TOP LEVEL");
		return 1;
	}

	return 2;
}
