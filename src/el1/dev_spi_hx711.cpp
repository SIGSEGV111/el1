#include "dev_spi_hx711.hpp"
#include "debug.hpp"
#include "util_bits.hpp"
#include <stdio.h>

#define IF_DEBUG_PRINTF(...) if(EL_UNLIKELY(DEBUG)) fprintf(stderr, __VA_ARGS__)

namespace el1::dev::spi::hx711
{
	using namespace util::bits;
	using namespace system::time;
	bool DEBUG = false;

	// static u32_t Discard2Bit(u64_t in)
	// {
	// 	u32_t out = 0;
	// 	for(unsigned i = 0; i < 24; i++)
	// 	{
	// 		out <<= 1;
	// 		out |= ((u32_t)(in >> 63) & 1);
	// 		in <<= 2;
	// 	}
	// 	return out;
	// }

	static u32_t Discard2Bit(const byte_t* const in)
	{
		u32_t out = 0;
		for(unsigned i = 0; i < 24; i++)
			SetBit((byte_t*)&out, i, GetBit(in, i * 2));
		return out;
	}

	static const TTime T_RESET = 0.0001; // 100µs
	// static const TTime T_BOOT  = 0.4000; // 400ms

	// We have to send 10101010 (0xAA) patterns as data to simulate a clock, thus we need two bits per virtual clock cycle
	// since we need to pull 24+3 bits (24 data + up to 3 bits for the config) from the chip, we have to allocate 7 bytes
	static const unsigned N_DATA_BYTES = 7;

	usys_t THX711::Read(float* const arr_items, const usys_t n_items_max)
	{
		if(booting)
		{
			if(timer.OnTick().IsReady())
			{
				// boot completed
				IF_DEBUG_PRINTF("DEBUG: boot completed => entering normal mode\n");
				booting = 0;

				if(this->irq != nullptr)
				{
					IF_DEBUG_PRINTF("DEBUG: disabling timer since we are using IRQ\n");
					timer.Stop();
				}
			}
			else
			{
				// still booting
				IF_DEBUG_PRINTF("DEBUG: chip is still booting up => return 0\n");
				return 0;
			}
		}

		if(n_items_max == 0)
			return 0;

		if(this->irq != nullptr)
		{
			if(this->irq->State())
			{
				IF_DEBUG_PRINTF("DEBUG: IRQ is still HIGH => no data available\n");
				return 0;
			}
			else
			{
				IF_DEBUG_PRINTF("DEBUG: IRQ is LOW => data might be available\n");
			}
		}

		// check the status of the DOUT/MISO line without sending a clock on MOSI/PD_CLK to not confuse the chip
		// if it reads back as 0xFF then the chip is still busy
		// if it reads back as 0x00 data was already waiting for retrival before we checked
		// if it reads back as anything in between the chip just finished as we checked (which is also fine)
		IF_DEBUG_PRINTF("DEBUG: polling chip DOUT level ...\n");
		byte_t data_ready = 0;
		this->device->ExchangeBuffers(&data_ready, &data_ready, 1);
		IF_DEBUG_PRINTF("DEBUG: data-ready = %s (%02hhx)\n", data_ready == 0xFF ? "no" : "yes", data_ready);
		if(data_ready == 0xFF)
		{
			if(timer.OnTick().IsReady())
			{
				// the timer expired too early (e.g. due to clock inaccuracies)
				// set it to check again in 1/10th of the normal sample-rate
				const double timeout = 1.0 / sample_rate / 50.0;
				IF_DEBUG_PRINTF("DEBUG: poll-timer expired too early => delaying by %0.3fms\n", timeout * 1000.0);
				timer.Start(TTime::Now(EClock::MONOTONIC) + timeout, 1.0 / sample_rate);
			}
			else if(this->irq != nullptr)
			{
				IF_DEBUG_PRINTF("DEBUG: IRQ must have been triggered by another device on the same line\n");
			}
			return 0;
		}

		if(this->irq == nullptr)
			timer.ReadMissedTicksCount(); // reset timer

		IF_DEBUG_PRINTF("DEBUG: fetching data ...\n");
		IF_DEBUG_PRINTF("DEBUG: adding %u extra clock cycles for config\n", config);

		// send 24 clock pulses
		// Generate the data clock pattern plus one check byte.
		// We always send and receive two bits per clock cycle. The HX711 applies the data on the rising edge
		// of the clock. The 1 bits will be sent as HIGH level and the 0 bits will be sent as LOW level on the MOSI line.
		byte_t buffer[N_DATA_BYTES + 1];
		memset(buffer, 0x00, sizeof(buffer));
		FillBits(buffer, 50 + config * 2, 0xAA);

		if(DEBUG) debug::Hexdump("DEBUG: tx-buffer      ", buffer, sizeof(buffer));
		this->device->ExchangeBuffers(buffer, buffer, sizeof(buffer));
		if(DEBUG) debug::Hexdump("DEBUG: rx-buffer      ", buffer, sizeof(buffer));

		if(this->irq != nullptr)
			this->irq->State(); // clear IRQ

		u32_t v2 = Discard2Bit(buffer);
		if(DEBUG) debug::Hexdump("DEBUG: after discard  ", &v2, 4);
 		v2 = be32toh(v2);
		if(DEBUG) debug::Hexdump("DEBUG: host endianity ", &v2, 4);
		s64_t v3 = v2;
		v3 /= 256;
		IF_DEBUG_PRINTF("DEBUG: raw sensor-value = %d (%06x)\n", (int)v3, (int)v3);
		v3 -= (config == 0 ? 0x800000 : (config == 1 ? 0 : 0x400000)); // mid-point
		double v4 = v3;
		v4 /= (16777216.0 / 2.0);
		arr_items[0] = (float)v4;
		IF_DEBUG_PRINTF("DEBUG: processed sensor-value = %f\n", arr_items[0]);

		// uppon complete data readout the HX711 must return DOUT to HIGH state, thus the check byte must read 0xff
		// anything else would indicate an out-of-sync error
		if(buffer[N_DATA_BYTES] != 0xff)
		{
			fprintf(stderr, "WARNING: HX711 out-of-sync, result discarded => resetting chip ...\n");
			Reset();
			return 0;
		}

		IF_DEBUG_PRINTF("DEBUG: return 1\n");
		return 1;
	}

	const system::waitable::IWaitable* THX711::OnInputReady() const
	{
		if(booting || this->irq == nullptr)
			return &timer.OnTick();
		else
			return &this->irq->OnInputTrigger();
	}

	void THX711::Reset()
	{
		IF_DEBUG_PRINTF("DEBUG: resetting sensor ...\n");
		byte_t buffer = 0xFF;
		this->device->Clock(HZ_CLOCK_SPI_RESET); // resulting clock will be smaller or equal => no need to check
		this->device->ExchangeBuffers(&buffer, nullptr, 1);
		timer.Start(TTime::Now(EClock::MONOTONIC) + T_RESET, 1.0f / sample_rate);
		booting = 1;
		config = 0;

		const u64_t hz_clock_spi_eff = this->device->Clock(hz_clock);
		EL_ERROR(hz_clock_spi_eff < HZ_CLOCK_SPI_MIN || hz_clock_spi_eff > HZ_CLOCK_SPI_MAX, TException, TString::Format("SPI bus was unable to provide the requested clock frequency of %d Hz. Instead it configured the clock for %d Hz, which is out of the valid range for the HX711.", hz_clock, hz_clock_spi_eff));

		if(DEBUG)
		{
			fprintf(stderr, "DEBUG: hz_clock_spi_req = %lu KHz\n", (unsigned long)hz_clock / 1000UL);
			fprintf(stderr, "DEBUG: hz_clock_spi_eff = %lu KHz\n", (unsigned long)hz_clock_spi_eff / 1000UL);
		}
	}

	THX711::THX711(std::unique_ptr<ISpiDevice> device, std::unique_ptr<gpio::IPin> irq, const u32_t hz_clock, const float sample_rate) :
		device(std::move(device)), irq(std::move(irq)), timer(EClock::MONOTONIC),
		booting(1), config(0), hz_clock(hz_clock), sample_rate(sample_rate)
	{
		EL_ERROR(hz_clock < HZ_CLOCK_SPI_MIN || hz_clock > HZ_CLOCK_SPI_MAX, TInvalidArgumentException, "hz_clock", "hz_clock out of range");

		if(this->irq != nullptr)
		{
			this->irq->Mode(gpio::EMode::INPUT);
			this->irq->Trigger(gpio::ETrigger::FALLING_EDGE);
		}

		Reset();
	}
}

#undef IF_DEBUG_PRINTF
