#include "def.hpp"
#ifdef EL_OS_LINUX

#include "dev_spi_native.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <string.h>

namespace el1::dev::spi::native
{
	using namespace system::waitable;
	using namespace system::handle;
	using namespace dev::gpio;
	using namespace io::file;

	TNativeSpiBus* TNativeSpiDevice::Bus() const
	{
		return bus;
	}

	void TNativeSpiDevice::ForceChipEnable(const bool force)
	{
		if(force)
		{
			EL_ERROR(this->bus->IsBusy(), TException, "bus is currently busy - check IsBusy() and OnBusIdle()");
			this->bus->active_device = this;
			this->chip_enable_pin->State(false);
		}
		else
		{
			if(this->bus->active_device == this)
			{
				this->chip_enable_pin->State(true);
				this->bus->active_device = nullptr;
			}
		}
	}

	u64_t TNativeSpiDevice::Clock(const u64_t device_max_hz)
	{
		// FIXME: use /sys/devices/platform/soc/20204000.spi/spi_master/spi0/of_node/spidev@0/spi-max-frequency
		return this->hz_speed = device_max_hz;
	}

	void TNativeSpiDevice::ExchangeBuffers(const void* const tx_buffer, void* const rx_buffer, const usys_t n_bytes, const bool clean_signal)
	{
		EL_ERROR(clean_signal, TNotImplementedException);

		while(this->bus->IsBusy() && this->bus->active_device != this)
			this->bus->OnBusIdle().WaitFor();

		const bool forced = this->bus->active_device == this;
		this->bus->active_device = this;

		if(this->chip_enable_pin != nullptr)
			this->chip_enable_pin->State(false);

		struct spi_ioc_transfer xfer_cmd;
		memset(&xfer_cmd, 0, sizeof(xfer_cmd));

		xfer_cmd.tx_buf = (usys_t)tx_buffer;
		xfer_cmd.rx_buf = (usys_t)rx_buffer;
		xfer_cmd.len = n_bytes;
		xfer_cmd.delay_usecs = 0;
		xfer_cmd.speed_hz = (u32_t)this->hz_speed;
		xfer_cmd.bits_per_word = 8;

		EL_SYSERR(ioctl(this->bus->handle, SPI_IOC_MESSAGE(1), &xfer_cmd));

		if(!forced)
		{
			if(this->chip_enable_pin != nullptr)
				this->chip_enable_pin->State(true);
			this->bus->active_device = nullptr;
		}
	}

	TNativeSpiDevice::TNativeSpiDevice(TNativeSpiBus* const bus, std::unique_ptr<IPin> chip_enable_pin) : bus(bus), chip_enable_pin(std::move(chip_enable_pin)), hz_speed(0)
	{
		if(this->chip_enable_pin != nullptr)
		{
			this->chip_enable_pin->AutoCommit(true);
			this->chip_enable_pin->Mode(EMode::OUTPUT);
			this->chip_enable_pin->State(true);
		}
	}

	TNativeSpiDevice::~TNativeSpiDevice()
	{
		ForceChipEnable(false);
	}

	bool TNativeSpiBus::IsBusy() const
	{
		return this->active_device != nullptr;
	}

	const TMemoryWaitable<usys_t>& TNativeSpiBus::OnBusIdle() const
	{
		return on_idle;
	}

	std::unique_ptr<ISpiDevice> TNativeSpiBus::ClaimDevice(std::unique_ptr<IPin> chip_enable_pin)
	{
		return std::unique_ptr<ISpiDevice>(new TNativeSpiDevice(this, std::move(chip_enable_pin)));
	}

	TNativeSpiBus::TNativeSpiBus(TPath device) : handle(EL_SYSERR(open(device, O_RDWR|O_CLOEXEC|O_NOCTTY)), true), active_device(nullptr), on_idle((usys_t*)&active_device, (usys_t*)NEG1, NEG1)
	{
	}
}

#endif
