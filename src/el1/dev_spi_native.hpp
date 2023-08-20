#pragma once

#include "dev_spi.hpp"
#include "io_file.hpp"
#include "system_handle.hpp"

namespace el1::dev::spi::native
{
	using namespace io::types;

	class TNativeSpiBus;
	class TNativeSpiDevice;

	class TNativeSpiBus : public ISpiBus
	{
		friend class TNativeSpiDevice;
		protected:
			system::handle::THandle handle;
			TNativeSpiDevice* active_device;
			system::waitable::TMemoryWaitable<usys_t> on_idle;

		public:
			bool IsBusy() const final override;
			const system::waitable::TMemoryWaitable<usys_t>& OnBusIdle() const final override EL_GETTER;
			std::unique_ptr<ISpiDevice> ClaimDevice(std::unique_ptr<gpio::IPin> chip_enable_pin) final override;

			TNativeSpiBus(const TNativeSpiBus&) = delete;
			TNativeSpiBus(TNativeSpiBus&&) = delete;

			TNativeSpiBus(io::file::TPath device);
	};

	class TNativeSpiDevice : public ISpiDevice
	{
		protected:
			TNativeSpiBus* bus;
			std::unique_ptr<gpio::IPin> chip_enable_pin;
			u64_t hz_speed;

		public:
			TNativeSpiBus* Bus() const final override EL_GETTER;
			void ForceChipEnable(const bool) final override;
			u64_t Clock(const u64_t device_max_hz) final override;
			void ExchangeBuffers(const void* const tx_buffer, void* const rx_buffer, const usys_t n_bytes) final override;

			TNativeSpiDevice(const TNativeSpiDevice&) = delete;
			TNativeSpiDevice(TNativeSpiDevice&&) = delete;

			TNativeSpiDevice(TNativeSpiBus* const bus, std::unique_ptr<gpio::IPin> chip_enable_pin);
			~TNativeSpiDevice();
	};
}
