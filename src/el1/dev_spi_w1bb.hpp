#pragma once

#include "dev_w1.hpp"
#include "dev_spi.hpp"
#include "system_time.hpp"
#include "io_collection_list.hpp"

namespace el1::dev::spi::w1bb
{
	/*
	 * This class bitbangs the one-wire (w1) protocol via the SPI bus.
	 * For this to work you need to connect the MOSI via a diode (or MOSFET/transistor)
	 * to the DATA line and the MISO directly to the DATA line. Additionally you need
	 * a pull-up resistor (about 4.7kOhm) to pull the DATA line to VCC.
	 * The resistor can vary depending on the power requirements of your devices and
	 * the length (=> capacity) of the bus.
	 * If you have a buffer or level-shifter with a chip-enable input or two MOSFETs
	 * chained together you can even continue to use the SPI bus for other devices.
	 * Bitbanging the one-wire bus via a hardware SPI is very reliable and
	 * CPU efficient compared to bitbanging it in software. Naturally it cannot
	 * compete against a native one-wire bus controller chip... except in price.
	 *
	 * If you use a MOSFET/transistor instead of a diode you need to invert the tx-polarity!
	 * Also check what the idle state of your MOSI pin is. If it is DRIVE LOW (with diode) or
	 * PULL/DRIVE HIGH (with MOSFET/transistor) you might be in trouble. Because the idle state of the
	 * one-wire bus must be PULL HIGH by the resistor. One-wire devices draw power from the bus
	 * after all and they can't do that if there is none.
	 *
	 * MOSFET or transistor vs. diode:
	 * Both work without issues. However for long bus systems with high capacitance, a MOSFET/transistor
	 * will allow you to use a pull-up resistor with lower value and thus a higher current. GPIOs are
	 * usually fairly limited in their current sink capability. Also a transistor or MOSFET is usually
	 * much more robust compared to a GPIO and thus can better withstand over-current/-voltage spikes.
	 *
	 * Raspberry Pi notes:
	 * The idle state of the MOSI pin is LOW, which is good if you use a MOSFET or transistor.
	 * The linux kernel driver spi-bcm2835 only uses DMA when the transfer is sufficiently large (>=96 bytes) and
	 * when `/sys/module/spi_bcm2835/parameters/polling_limit_us` has been set to 0 (or the computed transfer time is longer).
	 * This is important since in polling- or IRQ-mode (without DMA) there is a gap between data words sent over the wire.
	 * This gap breaks the strict timings for overdrive speed and also drives low the data line and thus
	 * ruins the entire signal. You must set `min_transfer_bytes` to 96 and `polling_limit_us` to 0 on
	 * the Raspberry Pi to enforce DMA. For details search for "BCM2835_SPI_DMA_MIN_LENGTH" in the kernel source.
	 *
	 * Strong pull-up:
	 * Many devices need extra power, which the regular pull-up resistor cannot provide.
	 * You can either route dedicated power lines to those devices (if supported) or implement a strong pull-up.
	 * Most devices just need a few mA extra which could be provided by a GPIO port (but check the datasheets!).
	 * However some drivers/applications address multiple devices at the same time and thus multiple devices might
	 * require extra power at the same time - make sure that your GPIO can handle this.
	 * Alternatively you can connect a dedicated GPIO port to a P-MOSFET which will then provide the power to the bus.
	 *
	 * SPI clock frequency:
	 * - regular speed: 100KHz
	 * - overdrive speed: 1MHz
	 * Make sure your level-shifter/MOSFET/transistor/diode can run at these speeds.
	 *
	 * Basic protocol description: https://en.wikipedia.org/wiki/1-Wire
	 * More info: https://developer.electricimp.com/resources/onewire
	 * https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html (AN126)
	 * https://twoerner.blogspot.com/2021/01/device-enumeration-on-1-wire-bus.html (EnumRom)
	 */

	using namespace w1;

	class TW1BbBus;
	class TW1BbDevice;

	static const bool DEBUG = false;

	enum class EStrongPullMode : u8_t
	{
		DIRECT_GPIO = 0,	// switch between OUTPUT-HIGH (enabled) and DISABLED (disabled)
		P_MOSFET = 1,		// switch between OUTPUT-LOW (enabled) and OUTPUT-HIGH (disabled)
		MISO = 2			// switch between OUTPUT-HIGH (enabled) and ALTERNATE FUNCTION SPI MISO (disabled)
	};

	class TW1BbDevice : public IW1Device
	{
		protected:
			TW1BbBus* const w1bus;
			const uuid_t uuid;
			ESpeed speed;

		public:
			void Read(const u8_t cmd, void* const arr_data, const u8_t n_data) final override;
			void Write(const u8_t cmd, const void* const arr_data, const u8_t n_data) final override;

			IW1Bus* Bus() const final override EL_GETTER;
			uuid_t UUID() const final override EL_GETTER;
			ESpeed Speed() const final override EL_GETTER;
			void Speed(const ESpeed new_speed) final override EL_SETTER;

			TW1BbDevice(TW1BbBus* const w1bus, const uuid_t uuid);
			~TW1BbDevice();
	};

	class TTransaction
	{
		protected:
			TW1BbBus* const w1bus;
			io::collection::list::array_t<byte_t> buffer;
			unsigned idx_buffer_next;

			// all data in the buffer is stored and returned encoded, this function encodes the data and places it in the buffer
			void Encode(const void* const arr_data, const unsigned n_data, const unsigned idx_buffer);

		public:
			void AddPattern(const byte_t pattern, const unsigned n_bytes);
			void AddPattern(const byte_t pattern, const system::time::TTime time);
			void AddDelay(const unsigned n_bytes);
			void AddDelay(const system::time::TTime time);
			unsigned AddReset();	// 2-byte presence pulse
			void AddWrite(const void* const arr_data, const unsigned n_data);
			unsigned AddRead(const unsigned n_data); // 8*n_data encoded data
			void AddCommand(const u8_t code);
			void AddSkipRom();
			void AddMatchRom(const uuid_t uuid);
			unsigned AddEnumRom(const uuid_t uuid); // (2 * bit-read + 1 * bit-write) * 56
			unsigned AddReadRom(); // 8-byte encoded ROM

			// all data in the buffer is stored and returned encoded, this function decodes the data from the buffer
			void Decode(const unsigned idx_buffer, void* const arr_data, const unsigned n_data);

			// computes the number of matching UUID bits from the result of the EnumRom command
			u8_t ComputeMatchingBits(const unsigned idx_enum_rom, const u8_t idx_start, const bool allow_forks);

			// returns whether or not any device was left in the search at this bit-position and whether or not these devices had a one at this position or a zero (or both)
			bool EnumRomBitInfo(const unsigned idx_enum_rom, const u8_t idx_bit, bool& has_one, bool& has_zero);

			// locks the bus and executes the transaction
			// afterwards the buffer contains the returned data at the specified positions
			void Execute();

			// clears the buffer of transaction so it can be reused
			void Clear();

			TTransaction(TW1BbBus* const w1bus, io::collection::list::array_t<byte_t> buffer);
	};

	class TW1BbBus : public IW1Bus
	{
		friend class TW1BbDevice;
		friend class TTransaction;
		protected:
			const std::unique_ptr<ISpiDevice> spidev;
			const std::unique_ptr<gpio::IPin> strong_pullup;
			io::collection::list::TList<TW1BbDevice*> claimed_devices;
			system::time::TTime pause_until;

			const u32_t min_transfer_bytes : 8;
			const s32_t miso_altfunc : 18;
			const u32_t invert_tx_polarity : 1;
			const u32_t max_allowed_speed : 1;
			const u32_t strong_pull_mode : 2;
			u32_t strong_pull_state : 1;
			u32_t selected_speed : 1;

			void ExchangeBuffers(byte_t* const tx_buffer, byte_t* const rx_buffer, const usys_t n_bytes);
			void StrongPullup(const bool state);

			static void Scan(TTransaction& tr, io::collection::list::TList<uuid_t>& list, u8_t idx_bit, uuid_t uuid);

		public:
			bool HasStrongPullUp() const final override EL_GETTER { return strong_pullup != nullptr; }
			bool Reset() final override;

			std::unique_ptr<IW1Device> ClaimDevice(const uuid_t uuid) final override;
			io::collection::list::TList<uuid_t> Scan(const ESpeed speed = ESpeed::REGULAR) final override;

			void PauseBus(const system::time::TTime duration) final override;
			system::time::TTime PausedUntil() const final override EL_GETTER;

			TW1BbBus(std::unique_ptr<ISpiDevice> spidev, const bool invert_tx_polarity, const ESpeed max_allowed_speed, const u8_t min_transfer_bytes = 0);
			TW1BbBus(std::unique_ptr<ISpiDevice> spidev, const bool invert_tx_polarity, const ESpeed max_allowed_speed, std::unique_ptr<gpio::IPin> strong_pullup, const EStrongPullMode strong_pull_mode, const int miso_altfunc, const u8_t min_transfer_bytes = 0);
			~TW1BbBus();
	};
}
