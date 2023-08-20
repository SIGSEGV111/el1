#pragma once

#include "dev_spi.hpp"
#include "io_file.hpp"
#include "system_handle.hpp"

namespace el1::dev::spi::native
{
	using namespace io::types;

	class TNativeSpiBus;
	class TNativeSpiDevice;

	/*
	 * Raspberry Pi notes:
	 * The idle state of the MOSI pin is LOW.
	 * The linux kernel driver spi-bcm2835 only uses DMA when the transfer is sufficiently large (>=96 bytes) and
	 * when `/sys/module/spi_bcm2835/parameters/polling_limit_us` has been set to 0 (or the computed transfer time is longer).
	 * This is important since in polling- or IRQ-mode (without DMA) there is a gap between data words sent over the wire.
	 * This gap breaks strict timings and also drives low the data line.
	 * You must set `min_transfer_bytes` to 96 and `polling_limit_us` to 0 on the Raspberry Pi to enforce DMA.
	 * For details search for "BCM2835_SPI_DMA_MIN_LENGTH" in the kernel source.
	 */
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
			void ExchangeBuffers(const void* const tx_buffer, void* const rx_buffer, const usys_t n_bytes, const bool clean_signal = false) final override;

			TNativeSpiDevice(const TNativeSpiDevice&) = delete;
			TNativeSpiDevice(TNativeSpiDevice&&) = delete;

			TNativeSpiDevice(TNativeSpiBus* const bus, std::unique_ptr<gpio::IPin> chip_enable_pin);
			~TNativeSpiDevice();
	};
}
