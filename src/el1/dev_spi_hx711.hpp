#pragma once

#include "dev_spi.hpp"
#include "system_time_timer.hpp"

namespace el1::dev::spi::hx711
{
	using namespace io::types;
	extern bool DEBUG;

	// 24-Bit Analog-to-Digital Converter (ADC) for Weigh Scales

	// connect DOUT to MISO (through a bus isolator if required)
	// connect PD_SCK to MOSI (through a bus isolator if required)
	// connect DOUT to IRQ (optional) - use a diode if the line is shared

	// The HX711 has no chip-enable input, thus you will have to come up with a solution for that on your
	// own if you want multiple devices on the same bus (use a bus-isolator / level-shifter with !CS input).
	// Make sure that the PD_SCK line is LOW on idle (e.g. by a weak pull-down resistor).
	// The HX711 will reset if PD_SCK is HIGH for more than 60µs.
	// The chip drives the DOUT line LOW when data is ready and drives it HIGH while busy.
	// This can be used as an IRQ. Make sure your IRQ line is not blocked by your
	// level-shifter or whatever you use to isolate the HX711 from the other devices.
	// This driver supports sharing the IRQ line with other devices (don't forget the diode!).

	// Since for the purpose of data transfers the effective clock speed is half the
	// SPI clock (0xAA pattern to emulate a clock) and the lowest allowed effective clock speed is
	// 20 KHz (to not cause a reset by a HIGH pulse), we cannot go below 40 KHz SPI clock.
	// The HX711 needs 100ns to apply the output data on DOUT after a rising clock flank.
	// This results in a theoretical minimum clock cycle time of 200ns => 5 MHz and this
	// in turn results in a theoretical maximum SPI clock of 10 MHz.
	// In practice we will use 1/10 of that for increased reliability.
	// min: 20KHz, max:  5 MHz (virtual)
	// min: 40KHz, max: 10 MHz (SPI)
	static const u32_t HZ_CLOCK_SPI_MIN   =    40000;
	static const u32_t HZ_CLOCK_SPI_MAX   = 10000000;

	// For the reset we choose to wait for 100µs instead of only 60µs since it makes
	// the math simpler and also adds to reliability. The calculated clock speed for reset
	// ensures that a single 0xFF byte when sent over the wire will pull the PD_SCK HIGH for 100µs.
	static const u32_t HZ_CLOCK_SPI_RESET =    80000;


	class THX711 : public io::stream::ISource<float>
	{
		protected:
			std::unique_ptr<ISpiDevice> device;
			std::unique_ptr<gpio::IPin> irq;
			system::time::timer::TTimer timer;
			u32_t	booting : 1,
					config: 2;
			const u32_t hz_clock : 29;
			const float sample_rate;

		public:
			enum class EConfig
			{
				CH_A_64 = 2,	// select channel A with x64 gain
				CH_A_128 = 0,	// select channel A with x128 gain
				CH_B_32 = 1		// select channel B with x32 gain
			};

			// resets the chip and config (CH_A_128)
			// takes between 50-500ms (depending on clock and RATE pin)
			// Read() will return 0 until the chip is ready => use OnInputReady()
			void Reset();

			usys_t Read(float* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			const system::waitable::IWaitable* OnInputReady() const final override;

			// configuration changes take effect AFTER the next successful Read()
			void Configuration(const EConfig new_config) { config = (u32_t)new_config; }
			EConfig Configuration() const EL_GETTER { return (EConfig)config; }

			THX711(std::unique_ptr<ISpiDevice> device, std::unique_ptr<gpio::IPin> irq = nullptr, const u32_t hz_clock = HZ_CLOCK_SPI_MAX / 10, const float sample_rate = 10.0f);
	};
}
