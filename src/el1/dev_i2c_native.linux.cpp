#include "def.hpp"
#ifdef EL_OS_LINUX

#include "dev_i2c_native.hpp"
#include "error.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include "io_file.hpp"
#include <endian.h>

namespace el1::dev::i2c::native
{
	using namespace io::file;
	using namespace io::stream;

	usys_t TDevice::Read(byte_t* const arr_items, const usys_t n_items_max)
	{
		return stream.Read(arr_items, n_items_max);
	}

	const system::waitable::IWaitable* TDevice::OnInputReady() const
	{
		return stream.OnInputReady();
	}

	iosize_t TDevice::WriteOut(ISink<byte_t>& sink, const iosize_t n_items_max, const bool allow_recursion)
	{
		return stream.WriteOut(sink, n_items_max, allow_recursion);
	}

	usys_t TDevice::BlockingRead(byte_t* const arr_items, const usys_t n_items_max, system::time::TTime timeout, const bool absolute_time)
	{
		return stream.BlockingRead(arr_items, n_items_max, timeout, absolute_time);
	}

	void TDevice::ReadAll(byte_t* const arr_items, const usys_t n_items)
	{
		stream.ReadAll(arr_items, n_items);
	}

	usys_t TDevice::Write(const byte_t* const arr_items, const usys_t n_items_max)
	{
		return stream.Write(arr_items, n_items_max);
	}

	const system::waitable::IWaitable* TDevice::OnOutputReady() const
	{
		return stream.OnOutputReady();
	}

	iosize_t TDevice::ReadIn(ISource<byte_t>& source, const iosize_t n_items_max, const bool allow_recursion)
	{
		return stream.ReadIn(source, n_items_max, allow_recursion);
	}

	void TDevice::Flush()
	{
		stream.Flush();
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
		return sc;
	}

	TDevice::TDevice(TBus* const bus, const u8_t address, const ESpeedClass sc) : stream(bus->device), bus(bus), address(address), sc(sc)
	{
		this->bus->claimed_addresses.Add(this->address, this);
		EL_SYSERR(ioctl(stream.Handle(), I2C_SLAVE, this->address));
	}

	TDevice::~TDevice()
	{
		this->bus->claimed_addresses.Remove(this->address);
	}

	ESpeedClass TBus::MaxSupportedSpeed() const
	{
		TPath path = TString("/sys/bus/i2c/devices/") + device.FullName() + "/of_node/clock-frequency";
		if(path.Exists())
		{
			TFile file(path);

			u32_t clock;
			file.ReadAll((byte_t*)&clock, sizeof(clock));
			clock = be32toh(clock);

			return (ESpeedClass)clock;
		}
		else
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
