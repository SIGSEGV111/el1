#include <el1/dev_gpio_dcf77.hpp>
#include <el1/dev_gpio_native.hpp>
#include <el1/error.hpp>
#include <el1/io_file.hpp>
#include <el1/system_cmdline.hpp>
#include <el1/system_task.hpp>
#include <el1/system_time.hpp>

#include <cstdio>
#include <ctime>
#include <sys/shm.h>

using namespace el1::io::types;
using namespace el1::system::time;

static const key_t CHRONY_SHM_KEY = 0x4e545030;

struct chrony_shm_t
{
	int mode;
	volatile int count;
#if defined(_TIME_BITS) && _TIME_BITS == 64
	unsigned clockTimeStampSec;
#else
	time_t clockTimeStampSec;
#endif
	int clockTimeStampUSec;
#if defined(_TIME_BITS) && _TIME_BITS == 64
	unsigned receiveTimeStampSec;
#else
	time_t receiveTimeStampSec;
#endif
	int receiveTimeStampUSec;
	int leap;
	int precision;
	int nsamples;
	volatile int valid;
	unsigned clockTimeStampNSec;
	unsigned receiveTimeStampNSec;
	unsigned top_clockTimeStampSec;
	unsigned top_receiveTimeStampSec;
	int dummy[6];
};

static chrony_shm_t* chrony_shm = nullptr;
static TTime timestamp_dcf77 = 0;
static TTime timestamp_system = 0;
static bool leap_announce = false;

static void attachSharedMemory(const key_t unit)
{
	using namespace el1::error;

	const int id = EL_SYSERR(shmget(CHRONY_SHM_KEY + unit, sizeof(chrony_shm_t), 0));
	chrony_shm = static_cast<chrony_shm_t*>(EL_SYSERR(shmat(id, nullptr, 0)));
	chrony_shm->valid = 0;
	__sync_synchronize();
	chrony_shm->mode = 1;
	chrony_shm->leap = 3;
	chrony_shm->precision = -20;
	chrony_shm->nsamples = 1;
	chrony_shm->count = 0;
}

static void updateSharedMemory()
{
	const s64_t dcf_seconds = timestamp_dcf77.Seconds();
	const s64_t system_seconds = timestamp_system.Seconds();

	__sync_synchronize();
	chrony_shm->valid = 0;
	chrony_shm->count = chrony_shm->count + 1;
	__sync_synchronize();
	chrony_shm->clockTimeStampSec = static_cast<decltype(chrony_shm->clockTimeStampSec)>(dcf_seconds);
	chrony_shm->clockTimeStampUSec = static_cast<int>(timestamp_dcf77.Attoseconds() / 1000000000000LL);
	chrony_shm->clockTimeStampNSec = static_cast<unsigned>(timestamp_dcf77.Attoseconds() / 1000000000LL);
	chrony_shm->receiveTimeStampSec = static_cast<decltype(chrony_shm->receiveTimeStampSec)>(system_seconds);
	chrony_shm->receiveTimeStampUSec = static_cast<int>(timestamp_system.Attoseconds() / 1000000000000LL);
	chrony_shm->receiveTimeStampNSec = static_cast<unsigned>(timestamp_system.Attoseconds() / 1000000000LL);
	chrony_shm->top_clockTimeStampSec = static_cast<unsigned>(static_cast<u64_t>(dcf_seconds) >> 32U);
	chrony_shm->top_receiveTimeStampSec = static_cast<unsigned>(static_cast<u64_t>(system_seconds) >> 32U);
	chrony_shm->leap = leap_announce ? 1 : 0;
	__sync_synchronize();
	chrony_shm->count = chrony_shm->count + 1;
	__sync_synchronize();
	chrony_shm->valid = 1;
	__sync_synchronize();
}

static void printTimestamp(const char* const label, const ::tm& time, const u64_t nanoseconds)
{
	char date_buffer[96];
	std::strftime(date_buffer, sizeof(date_buffer), "%a, %d %b %Y %T %z", &time);
	std::printf("%s: %s.%09llu", label, date_buffer, static_cast<unsigned long long>(nanoseconds));
}

static void onUpdate(const ::tm& decoded_time, const u64_t nanoseconds, const bool new_leap_announce)
{
	timestamp_system = TTime::Now(EClock::REALTIME);
	leap_announce = new_leap_announce;

	::tm mutable_decoded_time = decoded_time;
	timestamp_dcf77 = TTime(std::mktime(&mutable_decoded_time), static_cast<s64_t>(nanoseconds) * 1000000000LL);

	if(chrony_shm != nullptr)
	{
		updateSharedMemory();
	}

	const time_t system_seconds = static_cast<time_t>(timestamp_system.Seconds());
	::tm system_time = {};
	localtime_r(&system_seconds, &system_time);
	const TTime delta = timestamp_dcf77 - timestamp_system;

	std::putchar('\n');
	printTimestamp("CLOCK", system_time, static_cast<u64_t>(timestamp_system.Attoseconds() / 1000000000LL));
	std::putchar('\n');
	printTimestamp("DCF77", decoded_time, nanoseconds);
	std::printf(", delta=%f ms\n", delta.ConvertToF(EUnit::MILLISECONDS));
}

static void onTick(const TTime&, const int status)
{
	switch(status)
	{
		case 0:
		case 1:
			std::putchar('.');
			break;
		case -1:
			std::putchar('S');
			break;
		case -2:
			std::putchar('T');
			break;
		case -3:
			std::putchar('G');
			break;
		default:
			std::putchar('?');
			break;
	}
	std::fflush(stdout);
}

int main(const int argc, char* argv[])
{
	using namespace el1::dev::gpio::dcf77;
	using namespace el1::dev::gpio::native;
	using namespace el1::error;
	using namespace el1::io::file;
	using namespace el1::system::cmdline;
	using namespace el1::system::task;

	try
	{
		TPath path_gpio_chip = "/dev/gpiochip0";
		s64_t signal_line = -1;
		s64_t chrony_shm_index = -1;
		f64_t distance = 0;

		ParseCmdlineArguments(argc, argv,
			THelpArgument("Decode a DCF77 signal from a GPIO line."),
			TFlagArgument(&DEBUG, 'd', "debug", "", "Enable decoder debug output"),
			TPathArgument(&path_gpio_chip, EObjectType::CHAR_DEVICE, ECreateMode::OPEN, 'G', "gpio-chip", "", true, false, "GPIO character device"),
			TIntegerArgument(&signal_line, 'g', "gpio", "", false, false, "GPIO line index carrying the DCF77 signal"),
			TFloatArgument(&distance, 'm', "distance", "", true, false, "Distance in metres from the DCF77 transmitter"),
			TIntegerArgument(&chrony_shm_index, 's', "shm-index", "", true, false, "Chrony/NTP shared-memory unit; -1 disables output")
		);

		EL_ERROR(signal_line < 0, TInvalidArgumentException, "gpio", "non-negative GPIO line index");
		EL_ERROR(chrony_shm_index < -1, TInvalidArgumentException, "shm-index", "-1 or a non-negative unit number");
		EL_ERROR(distance < 0, TInvalidArgumentException, "distance", "non-negative distance");

		if(chrony_shm_index >= 0)
		{
			attachSharedMemory(static_cast<key_t>(chrony_shm_index));
		}

		TNativeGpioController gpio_controller(TFile(path_gpio_chip, TAccess::RW));
		TDCF77 dcf77(gpio_controller.ClaimPin(static_cast<usys_t>(signal_line)), distance);
		dcf77.OnUpdate() += onUpdate;
		dcf77.OnTick() += onTick;
		TFiber::Self()->Stop();
		return 0;
	}
	catch(const shutdown_t&)
	{
		return 0;
	}
	catch(const IException& exception)
	{
		exception.Print("TOP LEVEL");
		return 1;
	}
}
