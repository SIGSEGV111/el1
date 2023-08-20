#include "dev_i2c_opt3001.hpp"
#include "io_text_string.hpp"
#include <stdio.h>
#include <math.h>
#include <endian.h> // FIXME: Linux - other systems might not have it

namespace el1::dev::i2c::opt3001
{
	using namespace gpio;
	using namespace io::text::string;

	void TOPT3001::WriteRegister(const u8_t index, const u16_t value)
	{
		if(DEBUG) fprintf(stderr, "TOPT3001::WriteRegister(index = 0x%02hhx, value = 0x%04hx)\n", index, value);
		const u16_t tmp_value = htobe16(value);
		u8_t buffer[3];
		buffer[0] = index;
		memcpy(buffer + 1, &tmp_value, 2);
		this->regindex_cache = (u8_t)-1;
		this->device->WriteAll(buffer, 3);
		this->regindex_cache = index;
	}

	u16_t TOPT3001::ReadRegister(const u8_t index) const
	{
		if(index != this->regindex_cache)
		{
			this->device->WriteAll(&index, 1);
			this->regindex_cache = index;
		}

		u16_t value;
		this->device->ReadAll((byte_t*)&value, 2);
		value = be16toh(value);

		if(DEBUG) fprintf(stderr, "TOPT3001::ReadRegister(index = 0x%02hhx) => 0x%04hx)\n", index, value);

		return value;
	}

	bool TOPT3001::IsBusy() const
	{
		this->config_cache.reg = ReadRegister(0x01);
		return this->config_cache.m != 0;
	}

	static const u16_t CONFIG_MASK = 0b1111111000011111;

	void TOPT3001::Configure(const config_t new_config)
	{
		if((this->config_cache.reg & CONFIG_MASK) != (new_config.reg & CONFIG_MASK))
		{
			WriteRegister(0x01, new_config.reg);
			this->config_cache = new_config;
		}
	}

	void TOPT3001::Reset()
	{
		const u8_t reset_code = 0x06;
		this->device->WriteAll(&reset_code, 1);
		this->regindex_cache = (u8_t)-1;
		this->config_cache.reg = ReadRegister(0x01);

		if(DEBUG)
		{
			fprintf(stderr, "TOPT3001::Reset(): config = 0x%04hx, ManufacturerID = 0x%04hx, DeviceID = 0x%04hx\n", this->config_cache.reg, ManufacturerID(), DeviceID());
		}
		else
		{
			EL_ERROR(this->config_cache.reg != 0xC810, TException, TString::Format("failed to reset OPT3001 device at address 0x%x", this->device->Address()));
		}

		this->irq->AutoCommit(true);
		this->irq->Mode(EMode::INPUT);
		this->irq->Trigger(ETrigger::FALLING_EDGE);
		this->irq->State(); // clear IRQ
	}

	bool TOPT3001::Fetch()
	{
		this->config_cache.reg = ReadRegister(0x01);
		if(this->config_cache.crf == 0)
			return false;

		EL_ERROR(this->config_cache.ovf != 0 && this->config_cache.rn == 0b1100, TException, "chip reported overflow while in automatic full-scale mode.");

		const u16_t reg = ReadRegister(0x00);
		const u16_t exponent = (reg & 0xf000) >> 12;
		const u16_t mantissa = (reg & 0x0fff);

		const float lsb = 0.01f * powf(2, exponent);
		this->lux = lsb * (float)mantissa;

		if(DEBUG) fprintf(stderr, "TOPT3001::Fetch(): reg = %04hx ; exponent = %hu ; mantissa = %hu ; lsb = %f ; lux = %f\n", reg, exponent, mantissa, lsb, this->lux);

		return true;
	}

	void TOPT3001::TriggerConversion()
	{
		config_t new_config = this->config_cache;
		new_config.rn = 0b1100;
		new_config.m = 0b01;
		new_config.l = 1;
		new_config.pol = 0;
		new_config.me = 0;
		new_config.fc = 0;
		Configure(new_config);
	}

	float TOPT3001::Poll()
	{
		ConfigureDataReadyIRQ();
		TriggerConversion();

		do
		{
			EL_ERROR(this->config_cache.m == 0, TLogicException);	// WTF... chip went to shutdown (after data was converted?), but did not have any data ready?
			EL_ERROR(!this->irq->OnInputTrigger().WaitFor(8), TException, "OPT3001 IRQ timeout (expected IRQ within 8s after conversion start)");
		}
		while(!Fetch()); // We received the IRQ, but there was no ready data (yet) to be fetched. This can happen if the IRQ line is shared with other devices.

		return this->lux;
	}

	static u16_t FloatToReg(const float value)
	{
		// 2^exponent * 0.01 * mantissa = value
		// exponent = log2(value / 0.01 / 4095)

		EL_ERROR(value < 0.0f || value > 83865.60f, TInvalidArgumentException, "value", "value must positive and less or equal to 83865.60");
		const float min_req_lsb = value / 0.01f / 4095.0f;
		EL_ERROR(min_req_lsb > 20.48f, TLogicException);
		const long exponent = lrintf(ceilf(log2f(min_req_lsb)));
		const float lsb = 0.01f * powf(2, exponent);
		const long mantissa = lrintf(value / lsb);
		EL_ERROR(exponent > 11, TLogicException);
		EL_ERROR(mantissa > 4095, TLogicException);
		const u16_t reg = (u16_t)((exponent << 12) | mantissa);

		if(DEBUG) fprintf(stderr, "FloatToReg(value = %f) => %hd\n", value, reg);
		return reg;
	}

	void TOPT3001::ConfigureWindowIRQ(const float min, const float max, const ESampleCount n_samples)
	{
		WriteRegister(0x02, FloatToReg(min));
		WriteRegister(0x03, FloatToReg(max));

		config_t new_config = this->config_cache;
		new_config.rn = 0b1100;
		new_config.m = 0b11;
		new_config.l = 1;
		new_config.pol = 0;
		new_config.me = 0;
		new_config.fc = (u8_t)n_samples;
		Configure(new_config);
	}

	void TOPT3001::ConfigureComparisonOutput(const float min, const float max, const ESampleCount n_samples, const bool reverse_polarity)
	{
		WriteRegister(0x02, FloatToReg(min));
		WriteRegister(0x03, FloatToReg(max));

		config_t new_config = this->config_cache;
		new_config.rn = 0b1100;
		new_config.m = 0b11;
		new_config.l = 0;
		new_config.pol = reverse_polarity ? 1 : 0;
		new_config.me = 0;
		new_config.fc = (u8_t)n_samples;
		Configure(new_config);
	}

	void TOPT3001::ConfigureDataReadyIRQ()
	{
		WriteRegister(0x02, 0b1100000000000000);
	}

	void TOPT3001::ConversionTime(const EConversionTime time)
	{
		config_t new_config = this->config_cache;
		new_config.ct = time == EConversionTime::T100MS ? 0 : 1;
		Configure(new_config);
	}

	u16_t TOPT3001::ManufacturerID() const
	{
		return ReadRegister(0x7E);
	}

	u16_t TOPT3001::DeviceID() const
	{
		return ReadRegister(0x7F);
	}

	TOPT3001::TOPT3001(std::unique_ptr<i2c::II2CDevice> device, std::unique_ptr<gpio::IPin> irq) : device(std::move(device)), irq(std::move(irq)), lux(NAN), regindex_cache((u8_t)-1)
	{
		Reset();
	}

	TOPT3001::~TOPT3001()
	{
		Reset();
	}
}
