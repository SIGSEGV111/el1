#include "dev_dht22.hpp"
#include "error.hpp"
#include "debug.hpp"
#include "io_types.hpp"
#include <math.h>
#include <string.h>

namespace el1::dev::dht22
{
	using namespace io::types;
	using namespace error;
	using namespace debug;

	static bool Bit(const void* const arr, const usys_t index)
	{
		const usys_t idx_byte = index / 8;
		const u8_t idx_bit = index % 8;
		const u8_t mask = 1 << (7 - idx_bit);
		const bool value = (((const u8_t*)arr)[idx_byte] & mask) != 0;
		return value;
	}

	static unsigned CountBits(const void* const buffer, usys_t& bitpos, const unsigned min, const unsigned max, const bool value)
	{
		unsigned n = 0;

		for(; n < max && Bit(buffer, bitpos) == value; bitpos++)
			n++;

		EL_ERROR(n > max, TException, TString::Format("too many %s bits (max: %d, got: %d) at bitpos %d", max, n, value ? "HIGH" : "LOW", bitpos));
		EL_ERROR(n < min, TException, TString::Format("too few %s bits (min: %d, got: %d) at bitpos %d", min, n, value ? "HIGH" : "LOW", bitpos));

		return n;
	}

	static void SeekOverHeader(const void* const buffer, usys_t& bitpos)
	{
		EL_ERROR(!Bit(buffer, bitpos), TException, "first bit must be a high bit => circuit/bus error?");

		// initial no-mans-land phase; resistor will pull high for 20-40 µs until DHT22 takes control
		CountBits(buffer, bitpos, 1, 6, true);

		// 80µs low
		CountBits(buffer, bitpos, 6, 10, false);

		// 80µs high
		CountBits(buffer, bitpos, 6, 10, true);
	}

	static bool ParsePwmBit(const void* const buffer, usys_t& bitpos)
	{
		// 50µs low pulse (this is represented by 3 to 7 low bits, depending on alignment of bus clock and pulse modulation from DHT22)
		CountBits(buffer, bitpos, 3, 7, false);

		// 26µs to 70µs high pulse (this is represented by 1 to 9 high bits, depending on alignment of bus clock and pulse modulation from DHT22)
		const unsigned n = CountBits(buffer, bitpos, 1, 9, true);

		// a short pulse should be exactly 2 high bits and borderline even 3 high bits
		// a long pulse could at worst be interpreted as only 5 high bits and at most as 9 high bits
		// ...if the DHT22 and SPI bus are working to specs...

		// so if it is 4 high bits, we have no idea what this is supposed to mean
		EL_ERROR(n == 4, TException, TString::Format("misleading PWM data encountered (4 high bits) at bitpos %d", bitpos));

		return n >= 5;
	}

	static u8_t ParsePwmByte(const void* const buffer, usys_t& bitpos)
	{
		u8_t v = 0;
		for(int i = 7; i >= 0; i--)
			v |= ((ParsePwmBit(buffer, bitpos) ? 1 : 0) << i);
		return v;
	}

	static void ParsePwmData(const void* const buffer, float& celsius_temp, float& percent_humidity)
	{
		usys_t bitpos = 0;

		SeekOverHeader(buffer, bitpos);
		const u8_t humidity_int    = ParsePwmByte(buffer, bitpos);
		const u8_t humidity_dec    = ParsePwmByte(buffer, bitpos);
		const u8_t temperature_int = ParsePwmByte(buffer, bitpos);
		const u8_t temperature_dec = ParsePwmByte(buffer, bitpos);
		const u8_t checksum_rx     = ParsePwmByte(buffer, bitpos);
		const u8_t checksum_calc   = humidity_int + humidity_dec + temperature_int + temperature_dec;

		EL_ERROR(checksum_calc != checksum_rx, TException, TString::Format("checksum does not match (expected: %d, received: %d)", checksum_calc, checksum_rx));

		// write sensor readings
		percent_humidity = (humidity_int * 256 + humidity_dec) / 10.0f;
		celsius_temp = ((temperature_int & 0x7F) * 256 + temperature_dec) / 10.0f;
		if(temperature_int & 0x80)
			celsius_temp *= -1.0f;
	}

	TDHT22Spi::TDHT22Spi(std::unique_ptr<spi::ISpiDevice> spidev) : spidev(std::move(spidev)), temp(NAN), humidity(NAN)
	{
		this->spidev->Clock(100000); // 10µs/bit => 100KHz
	}

	void TDHT22Spi::Refresh()
	{
		// bit timing:
		//   50µs low
		//   30µs high => 0
		//   70µs high => 1
		//   max time: 50 + 70µs => 120µs
		//
		// start-signal must be more than 1ms low => 1.6ms
		// 1600µs / 10µs/bit => 160bits => 20byte for start-signal
		// 40bit data, each needs up to 120µs to transfer
		// 40bit * 120µs/bit => 4800µs
		// 4800µs / 10µs/bit => 480bit => 60byte data capture buffer
		// sensor ack is 20-40µs high + 80µs low + 80µs high => up to 200µs total => 20bit => ~3byte
		const unsigned n_start_signal = 20;
		const unsigned n_ack_signal = 3;
		const unsigned n_data = 60;
		const unsigned sz_buffer = n_start_signal + n_ack_signal + n_data + 10; // +10 => a couple of bytes extra
		u8_t tx[sz_buffer];
		u8_t rx[sz_buffer];

		try
		{
			// set the first 20 bytes to 0, to create a 1.6ms long low pulse at the start (each bit takes 10µs @ 100KHz)
			// set the remainder of the buffer to 1 to create a constant high value on the bus, which will be filtered out by the diode
			memset(tx, 0, n_start_signal);
			memset(tx + n_start_signal, 255, sz_buffer - n_start_signal);

			// clear RX buffer
			memset(rx, 0, sz_buffer);

			// sent TX buffer and at the same time receive RX buffer
			this->spidev->ExchangeBuffers(tx, rx, sz_buffer);

			// the first 'n_start_signal' bytes of the RX buffer must be zeros, or something is wrong with the circuit/bus
			for(unsigned i = 0; i < n_start_signal; i++)
				EL_ERROR(rx[i] != 0, TException, "non-zero bus voltage received during 1.6ms start-signal phase => circuit/bus error?");

			// parse the received PWM data (hand only the data part over, skip the start-signal part)
			ParsePwmData(rx + n_start_signal, this->temp, this->humidity);
		}
		catch(const IException& e)
		{
			if(DEBUG)
			{
				this->temp = NAN;
				this->humidity = NAN;

				e.Print("DHT22 refresh failed");
				Hexdump(tx, sz_buffer);
				Hexdump(rx, sz_buffer);
			}
			else
				EL_FORWARD(e, TException, TString::Format("Error while refreshing DHT22 sensor data.\n\ntx-buffer:\n%s\nrx-buffer:\n%s", HexdumpStr(tx, sz_buffer), HexdumpStr(rx, sz_buffer)));
		}
	}
}
