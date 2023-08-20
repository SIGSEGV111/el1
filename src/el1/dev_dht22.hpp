#pragma once

#include "dev_spi.hpp"

namespace el1::dev::dht22
{
	static const bool DEBUG = false;

	// The TDHT22Spi class requires that you connect MISO directly to the DATA line of the DHT22 and
	// MOSI via a diode to the DATA line. The diode makes it possible to drive the DATA line low from
	// the host, but not to drive it high. This allows the host to send the start-signal via MOSI and
	// then receive the data via MISO

	// You can use a level-shifter or buffer with a chip-enable input or two n-channel MOSFETs to make
	// the DHT22 a true SPI device. If you do not use such a buffer/level-shifter/MOSFET-chain then
	// your SPI bus becomes unuseable for other devices.

	// Driving the DHT22 this way has two major advantages: very high reliability (since all transactions
	// are done fully in hardware) and low cpu usage (no busy-loops, no time-critical code, etc.)

	// Circuit example: https://oshwlab.com/Neegu0Sh/dht22-spi-driver

	/*
	    simple circuit:

	    VCC ---- 10kOhm Resistor -------\
		MOSI --- <<(K)DIODE(A)<< ---\    \
		MISO ------------------------\----\--- DHT22-DATA
	*/

	class TDHT22Spi
	{
		protected:
			std::unique_ptr<spi::ISpiDevice> spidev;

			float temp;
			float humidity;

		public:
			TDHT22Spi(std::unique_ptr<spi::ISpiDevice> spidev);

			// temperature in °C
			float Temperature() const EL_GETTER { return temp; }

			// humidity in %RH (1-100)
			float Humidity() const EL_GETTER { return humidity; }

			void Refresh();
	};
}
