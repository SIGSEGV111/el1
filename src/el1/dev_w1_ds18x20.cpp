#include "dev_w1_ds18x20.hpp"
#include "io_text_string.hpp"
#include "system_task.hpp"

namespace el1::dev::w1::ds18x20
{
	using namespace io::text::string;
	using namespace system::task;

	bool TDS18X20::DetectBusPowered()
	{
		byte_t state;
		this->w1dev->Read(CMD_POWER_STATE, &state, 1);
		return (state & 1) == 0;
	}

	TString TDS18X20::ModelName(const EModel model)
	{
		switch(model)
		{
			case EModel::DS18B20:
				return L"DS18B20";
			case EModel::DS18S20:
				return L"DS18S20";
		}

		EL_THROW(TLogicException);
	}

	EModel TDS18X20::Model() const
	{
		return (EModel)this->w1dev->UUID().type;
	}

	TString TDS18X20::ModelName() const
	{
		return ModelName(this->Model());
	}

	float TDS18X20::Temperature() const
	{
		switch(Model())
		{
			case EModel::DS18S20:
			{
				EL_ERROR(this->ds18s20.count_per_degc == 0, TException, TString::Format(L"invalid temperature data in local cache (rom = %s, temp_raw = %d, count_remain = %d, count_per_degc = %d)", this->w1dev->UUID().ToString(), this->ds18s20.temp_raw, this->ds18s20.count_remain, this->ds18s20.count_per_degc));
				return (float)(this->ds18s20.temp_raw >> 1) - 0.25f + (float)(this->ds18s20.count_per_degc - this->ds18s20.count_remain) / (float)this->ds18s20.count_per_degc;
			}

			case EModel::DS18B20:
			{
				return this->ds18b20.temp_raw * 0.0625f;
			}
		}

		EL_THROW(TLogicException);
	}

	void TDS18X20::TriggerConversion()
	{
		this->w1dev->Write(CMD_TRIGGER_CONVERSION, nullptr, 0);
	}

	void TDS18X20::Fetch()
	{
		switch(Model())
		{
			case EModel::DS18S20:
			{
				scratchpad_ds18s20_t scratchpad;
				this->w1dev->Read(CMD_READ_SCRATCHPAD, &scratchpad, sizeof(scratchpad));
				const u8_t calculated_crc = CalculateCRC(&scratchpad, sizeof(scratchpad) - 1);
				EL_ERROR(scratchpad.crc != calculated_crc, TCrcMismatchException, this->w1dev->UUID(), scratchpad.crc, calculated_crc);
				this->ds18s20.temp_raw = scratchpad.temp_raw;
				this->ds18s20.count_remain = scratchpad.count_remain;
				this->ds18s20.count_per_degc = scratchpad.count_per_degc;
				return;
			}

			case EModel::DS18B20:
			{
				scratchpad_ds18b20_t scratchpad;
				this->w1dev->Read(CMD_READ_SCRATCHPAD, &scratchpad, sizeof(scratchpad));
				const u8_t calculated_crc = CalculateCRC(&scratchpad, sizeof(scratchpad) - 1);
				EL_ERROR(scratchpad.crc != calculated_crc, TCrcMismatchException, this->w1dev->UUID(), scratchpad.crc, calculated_crc);
				this->ds18b20.temp_raw = scratchpad.temp_raw;
				return;
			}
		}

		EL_THROW(TLogicException);
	}

	float TDS18X20::Poll()
	{
		this->TriggerConversion();
		TFiber::Sleep(0.75);
		this->Fetch();
		return this->Temperature();
	}

	TDS18X20::TDS18X20(std::unique_ptr<IW1Device> w1dev, const EPowerSource power_source) :
		w1dev(std::move(w1dev)),
		bus_powered(power_source == EPowerSource::PARASITIC || (power_source == EPowerSource::AUTO_DETECT && DetectBusPowered()))
	{
		ds18s20.temp_raw = 0;
		ds18s20.count_remain = 0;
		ds18s20.count_per_degc = 0;
		EL_ERROR(this->w1dev->UUID().type != (u8_t)EModel::DS18B20 && this->w1dev->UUID().type != (u8_t)EModel::DS18S20, TInvalidArgumentException, "w1dev", "w1dev does not point to a supported DS18X20 sensor");
		EL_ERROR(this->bus_powered && !this->w1dev->Bus()->HasStrongPullUp(), TException, TString::Format(L"Insufficient power for %s @ %s. W1-Bus lacks strong pullup and sensor is currently bus powered.", this->ModelName(), this->w1dev->UUID().ToString()));

		if(Model() == EModel::DS18B20)
			this->w1dev->Write(CMD_WRITE_SCRATCHPAD, "\xff\xff\xff", 3); // set 12bit resolution => 750ms conversion time
	}
}
