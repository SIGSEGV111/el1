#include "def.hpp"
#ifdef EL_OS_LINUX

#include "dev_i2c_native.hpp"
#include "error.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

namespace el1::dev::i2c::native
{
	using namespace io::file;

	usys_t TDevice::Read(byte_t* const arr_items, const usys_t n_items_max)
	{
		EL_ERROR(read(this->handle, arr_items, n_items_max) != (ssize_t)n_items_max, TSyscallException, errno);
		return n_items_max;
	}

	usys_t TDevice::Write(const byte_t* const arr_items, const usys_t n_items_max)
	{
		EL_ERROR(write(this->handle, arr_items, n_items_max) != (ssize_t)n_items_max, TSyscallException, errno);
		return n_items_max;
	}

	void TDevice::Flush()
	{
		EL_SYSERR(fsync(this->handle));
	}

	II2CBus* TDevice::Bus() const
	{
		return bus;
	}

	u8_t TDevice::Address() const
	{
		return address;
	}

	ESpeedClass TDevice::SpeedClass() const
	{
		// FIXME: use info from /sys/bus/i2c/devices/i2c-1/of_node/clock-frequency
		return sc;
	}

	TDevice::TDevice(TBus* const bus, const u8_t address, const ESpeedClass sc) : handle(EL_SYSERR(open(bus->device, O_RDWR|O_CLOEXEC|O_NOCTTY/*|O_SYNC*/)), true), bus(bus), address(address), sc(sc)
	{
		this->bus->claimed_addresses.Add(this->address, this);
		EL_SYSERR(ioctl(this->handle, I2C_SLAVE, this->address));
	}

	TDevice::~TDevice()
	{
		this->bus->claimed_addresses.Remove(this->address);
	}

	ESpeedClass TBus::MaxSupportedSpeed() const
	{
		return ESpeedClass::STD;
	}

	std::unique_ptr<II2CDevice> TBus::ClaimDevice(const u8_t address, const ESpeedClass sc_max)
	{
		return std::unique_ptr<II2CDevice>(new TDevice(this, address, (ESpeedClass)util::Min<u8_t>((u8_t)sc_max, (u8_t)MaxSupportedSpeed())));
	}

	TBus::TBus(TPath device) : device(device)
	{
	}
}

#endif
