#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include "dev_gpio.hpp"
#include "system_waitable.hpp"

namespace el1::dev::spi
{
	using namespace io::types;

	struct ISpiBus;
	struct ISpiDevice;

	struct ISpiDevice
	{
		virtual ~ISpiDevice() {};

		virtual ISpiBus* Bus() const EL_GETTER = 0;

		// true: forces the chip enable pin to low; false: automatic
		// normally the chip-enable pin is automatically controlled by ExchangeBuffers()
		// if true is passed as argument to this function the chip-enable pin is forced low (device enabled)
		// this locks the bus and prevents other devices from trasmitting data
		virtual void ForceChipEnable(const bool) = 0;

		// sets the clock speed when exchanging data with this device
		// argument "device_max_hz" is the the highest clock frequence the device supports
		// the function configures the bus for the highest supported speed which is smaller or equal to "device_max_hz"
		// this effective clock speed is then returned
		virtual u64_t Clock(const u64_t device_max_hz) = 0;

		// sends "tx_buffer" and receives "rx_buffer" at the same time
		// "n_bytes" specifies the size of the buffers
		// any of "tx_buffer" or "rx_buffer" can be nullptr
		// if "tx_buffer" is nullptr then zeros are sent
		// if "rx_buffer" is nullptr then received data is discarded
		// "tx_buffer" and "rx_buffer" can point to the same memory location without causing a conflict
		// data is sent before the buffer is overwritten by recveived data
		// this function blocks if the bus is locked by another device
		virtual void ExchangeBuffers(const void* const tx_buffer, void* const rx_buffer, const usys_t n_bytes, const bool clean_signal = false) = 0;
	};

	struct ISpiBus
	{
		// returns wheter or not the bus is currently locked by a device for data exchange
		virtual bool IsBusy() const  = 0;
		virtual const system::waitable::IWaitable& OnBusIdle() const EL_GETTER = 0;

		virtual std::unique_ptr<ISpiDevice> ClaimDevice(std::unique_ptr<gpio::IPin> chip_enable_pin) = 0;
	};
}
