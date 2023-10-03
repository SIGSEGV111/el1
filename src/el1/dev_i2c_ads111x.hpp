#pragma once

#include "def.hpp"
#include "dev_i2c.hpp"
#include "dev_gpio.hpp"
#include "system_time_timer.hpp"
#include "error.hpp"

namespace el1::dev::i2c::ads111x
{
	extern bool DEBUG;

	enum class EDataRate : u8_t
	{
		R8 = 0,
		R16 = 1,
		R32 = 2,
		R64 = 3,
		R128 = 4,
		R250 = 5,
		R475 = 6,
		R860 = 7,
	};

	static const u16_t ARR_DATARATE[] = { 8, 16, 32, 64, 128, 250, 475, 860 };

	enum class EPGA : u8_t
	{
		FSR_6144 = 0,
		FSR_4096 = 1,
		FSR_2048 = 2,
		FSR_1024 = 3,
		FSR_0512 = 4,
		FSR_0256 = 5,
	};

	static const u16_t ARR_FSR[] = { 6144, 4096, 2048, 1024, 512, 256 };

	enum class EChannel : u8_t
	{
		DIFF_A0_A1 = 0,
		DIFF_A0_A3 = 1,
		DIFF_A1_A3 = 2,
		DIFF_A2_A3 = 3,
		A0 = 4,
		A1 = 5,
		A2 = 6,
		A3 = 7,
	};

	enum class ECompQueue : u8_t
	{
		C1 = 0,
		C2 = 1,
		C4 = 2,
	};

	enum class EOpMode : u8_t
	{
		// The ADC will operate continuously.
		CONTINUOUS = 0,

		// The ADC has to be manually triggered for every conversion.
		SINGLE_SHOT = 1,
	};

	struct config_t
	{
		union
		{
			struct
			{
				u16_t
					comp_que : 2,
					comp_lat : 1,
					comp_pol : 1,
					comp_mode : 1,
					dr : 3,
					mode : 1,
					pga : 3,
					mux : 3,
					os : 1;
			};
			u16_t _word;
		};

		s16_t comp_low;
		s16_t comp_high;

		void OpMode(const EOpMode om);
		EOpMode OpMode() const EL_GETTER;

		void PGA(const EPGA p);
		EPGA PGA() const EL_GETTER;

		void Channel(const EChannel ch);
		EChannel Channel() const EL_GETTER;

		void DataRate(const EDataRate rate);
		EDataRate DataRate() const EL_GETTER;

		// This disables the IRQ and instead employs a software timer (based on data rate) to poll the ADC in continuous mode.
		// Keep in mind that the clock in the ADC chip and the software timer on the host are NOT synchronized - you might skip results and/or read the same result multiple times. Only single-shot mode will work reliably without IRQ.
		void DisableIrq();

		// This will configure the ALERT line as IRQ and the IRQ will trigger when a conversion completes and is ready to be read - this is the only "safe" option for continuous mode when every result must be read and only read once.
		// The IRQ line CANNOT be shared with other devices, since the ADS111X has no data-ready flag register, so there is no way to tell if the IRQ came from the ADS111X or another device on the same line.
		// TODO verify the bahavior of the OS flag in continuous mode
		void ConfigureDataReadyIrq();

		// This configures the ALERT line to indicate when the result of the conversion is below `low` or above `high` (this disables the IRQ). You probably also want to switch to continuous mode, but this is NOT done automatically.
		void ConfigureWindowComparator(const s16_t low, const s16_t high, const ECompQueue q = ECompQueue::C1);

		// This configures the ALERT line to indicate when the result is above `high` and only clears when the results goes below `low` (this disables the IRQ). You probably also want to switch to continuous mode, but this is NOT done automatically.
		void ConfigureHysteresisComparator(const s16_t low, const s16_t high, const ECompQueue q = ECompQueue::C1);

		bool HasIrq() const EL_GETTER;

		// converts the provided sensor value into Volts based on the selected FSR/PGA settings
		float ToVolts(const s16_t value) const EL_GETTER;

		config_t();
		config_t(const config_t&);
	};

	class TADS111X : public io::stream::ISource<s16_t>
	{
		protected:
			std::unique_ptr<II2CDevice> device;
			std::unique_ptr<gpio::IPin> irq;
			std::unique_ptr<system::time::timer::TTimer> timer;
			config_t current_config;

			void ReadConfig();
			void WriteConfig(const config_t new_config);

		public:
			usys_t Read(s16_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			const system::waitable::IWaitable* OnInputReady() const final override EL_GETTER;

			// only effective in single-shot mode
			// has no effect when called while a conversion is still in progress
			void TriggerConversion();

			// returns `true` if a conversion is in pogress, false otherwise
			bool Busy() const;

			// The device applies the config to the next conversion, any ongoing conversion will
			// continue to operate on the previous config. To reliably switch config in continuous
			// mode, you should apply the config and then discard the first result.
			void Config(const config_t new_config);
			const config_t& Config() const EL_GETTER;

			TADS111X(std::unique_ptr<II2CDevice> device, std::unique_ptr<gpio::IPin> irq);
	};
}
