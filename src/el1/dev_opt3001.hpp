#pragma once

#include "dev_i2c.hpp"
#include "dev_gpio.hpp"
#include "io_types.hpp"

namespace el1::dev::opt3001
{
	using namespace io::types;

	static const bool DEBUG = false;

	/*
	 * IRQ logic: the chip drives the IRQ line low when then IRQ is active.
	 * The IRQ line must be pulled-up by some resistor.
	 */

	enum class EConversionTime : u8_t
	{
		T100MS,	// 100ms/sample (higher sample rate)
		T800MS	// 800ms/sample (less noise)
	};

	enum class ESampleCount : u8_t
	{
		ONE = 0,
		TWO = 1,
		FOUR = 2,
		EIGHT = 3
	};

	class TOPT3001
	{
		protected:
			struct config_t
			{
				union
				{
					struct
					{
						u16_t
							fc : 2,
							me : 1,
							pol : 1,
							l : 1,
							fl : 1,
							fh : 1,
							crf : 1,
							ovf : 1,
							m : 2,
							ct : 1,
							rn : 4;
					};
					u16_t reg;
				};
			};

			std::unique_ptr<i2c::II2CDevice> device;
			std::unique_ptr<gpio::IPin> irq;
			float lux;
			mutable config_t config_cache;
			mutable u8_t regindex_cache;

			void WriteRegister(const u8_t index, const u16_t value);
			u16_t ReadRegister(const u8_t index) const;
			void Configure(const config_t new_config);

		public:
			float Lux() const EL_GETTER { return lux; }

			const system::waitable::IWaitable& OnIrq() const EL_GETTER { return irq->OnInputTrigger(); }

			bool IsBusy() const;

			void Reset();

			// fetches the latest sample
			// returns false is no new data was available, true otherwise
			bool Fetch();

			// triggers a new conversion
			// switches the chip into one-shot mode
			// fetch the result with Fetch()
			// conversion time depends on various random factors
			void TriggerConversion();

			// triggers a new conversion and blocks until the result is available
			// essentially does:
			// 1. ConfigureNewSampleIRQ()
			// 2. TriggerConversion();
			// 3. OnIrq().WaitFor();
			// 4. Fetch();
			// 5. return Lux();
			float Poll();

			// configures the sensor to trigger and latch the IRQ when the value goes below "min" or above "max"
			// clear the IRQ with Fetch()
			// this will also switch the chip into continuous mode
			// "n_samples" configure show many measurments need to violate the condition before the IRQ is triggered
			void ConfigureWindowIRQ(const float min, const float max, const ESampleCount n_samples);

			// configures the IRQ to signal the result of a comparison
			// if the sensor value is greater than "max" the IRQ is driven low
			// if the sensor value is smaller than "min" the IRQ is released and left floating (pulled-up by external resistor)
			// in between "min" and "max" the output is not changed from its previous state (hysteresis)
			// this will also switch the chip into continuous mode
			// "n_samples" configure show many measurments need to violate the condition before the output is changed
			// "reverse_polarity" reverses the polarity of the output signal (high when bigger than "max", low when smaller than "min", etc.)
			void ConfigureComparisonOutput(const float min, const float max, const ESampleCount n_samples, const bool reverse_polarity);

			// configures the sensor to trigger the IRQ when a new sample was acquired and converted
			// the IRQ will remain active until the data was fetched
			void ConfigureDataReadyIRQ();

			// configures the maximum sample rate/integration time
			// the device might take longer than the specified times due to repeatet attempts to find the best full-scale range
			void ConversionTime(const EConversionTime time) EL_SETTER;

			u16_t ManufacturerID() const;
			u16_t DeviceID() const;

			TOPT3001(std::unique_ptr<i2c::II2CDevice> device, std::unique_ptr<gpio::IPin> irq);
			~TOPT3001();
	};
}
