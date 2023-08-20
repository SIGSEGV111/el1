#include "dev_spi_w1bb.hpp"
#include "system_time_timer.hpp"
#include "debug.hpp"
#include <math.h>
#include <iostream>

namespace el1::dev::spi::w1bb
{
	using namespace system::time;
	using namespace system::time::timer;
	using namespace gpio;
	using namespace el1::debug;

	void TW1BbDevice::Read(const u8_t cmd, void* const arr_data, const u8_t n_data)
	{
		const unsigned sz_buffer = util::Max(this->w1bus->min_transfer_bytes, 11U + (9U + 1U + n_data) * 8U);
		byte_t buffer[sz_buffer];
		TTransaction tr(this->w1bus, array_t<byte_t>(buffer, sz_buffer));
		tr.Clear();
		tr.AddReset();
		tr.AddMatchRom(this->uuid);
		tr.AddCommand(cmd);
		const unsigned idx_read = tr.AddRead(n_data);
		tr.Execute();
		tr.Decode(idx_read, arr_data, n_data);
	}

	void TW1BbDevice::Write(const u8_t cmd, const void* const arr_data, const u8_t n_data)
	{
		const unsigned sz_buffer = util::Max(this->w1bus->min_transfer_bytes, 11U + (9U + 1U + n_data) * 8U);
		byte_t buffer[sz_buffer];
		TTransaction tr(this->w1bus, array_t<byte_t>(buffer, sz_buffer));
		tr.Clear();
		tr.AddReset();
		tr.AddMatchRom(this->uuid);
		tr.AddCommand(cmd);
		tr.AddWrite(arr_data, n_data);
		tr.Execute();
	}

	IW1Bus* TW1BbDevice::Bus() const { return w1bus; }
	uuid_t TW1BbDevice::UUID() const { return uuid; }
	ESpeed TW1BbDevice::Speed() const { return speed; }
	void TW1BbDevice::Speed(const ESpeed new_speed) { speed = new_speed; }

	TW1BbDevice::TW1BbDevice(TW1BbBus* const w1bus, const uuid_t uuid) : w1bus(w1bus), uuid(uuid), speed(ESpeed::REGULAR)
	{
		for(const TW1BbDevice* device : this->w1bus->claimed_devices)
			EL_ERROR(device->uuid == uuid, TException, "device already claimed");

		this->w1bus->claimed_devices.Append(this);
	}

	TW1BbDevice::~TW1BbDevice()
	{
		this->w1bus->claimed_devices.RemoveItem(this, 1);
	}

	/**********************************************/

	static const byte_t SPIBB_ZERO      = 0b00000011;
	static const byte_t SPIBB_ONE       = 0b01111111;
	static const byte_t SPIBB_READ_MASK = 0b01110000;

	// decodes a encoded bit and returns it's value (true = HIGH, false = LOW)
	static bool BitState(const u8_t encoded_bit)
	{
		return (encoded_bit & SPIBB_READ_MASK) == SPIBB_READ_MASK;
	}

	void TTransaction::Encode(const void* const arr_data, const unsigned n_data, const unsigned idx_buffer)
	{
		for(unsigned i = 0; i < n_data; i++)
			for(unsigned j = 0; j < 8U; j++)
				this->buffer[idx_buffer + i * 8U + j] = ((reinterpret_cast<const byte_t*>(arr_data)[i] & (1<<j)) != 0) ? SPIBB_ONE : SPIBB_ZERO;
	}

	void TTransaction::Decode(const unsigned idx_buffer, void* const arr_data, const unsigned n_data)
	{
		for(usys_t m = 0; m < n_data; m++)
		{
			byte_t b = 0;

			for(usys_t n = 0; n < 8U; n++)
			{
				// TODO optimize this...
				const usys_t i = m * 8U + n;
				const bool state = (this->buffer[idx_buffer + i] & SPIBB_READ_MASK) == SPIBB_READ_MASK;
				b |= ((state ? 1 : 0) << n);
			}

			reinterpret_cast<byte_t*>(arr_data)[m] = b;
		}
	}

	void TTransaction::AddPattern(const byte_t pattern, const unsigned n_bytes)
	{
		for(unsigned i = 0; i < n_bytes; i++)
			this->buffer[this->idx_buffer_next++] = pattern;
	}

	void TTransaction::AddPattern(const byte_t pattern, const TTime time)
	{
		const unsigned hz_spi = (ESpeed)this->w1bus->selected_speed == ESpeed::REGULAR ? 100000U : 1000000U;
		const unsigned n_bytes_per_second = hz_spi / 8U;
		const unsigned n_bytes = lrint(ceil((double)n_bytes_per_second * time.ConvertToF(EUnit::SECONDS)));
		this->AddPattern(pattern, n_bytes);
	}

	void TTransaction::AddDelay(const unsigned n_bytes)
	{
		this->AddPattern(0xff, n_bytes);
	}

	void TTransaction::AddDelay(const TTime time)
	{
		this->AddPattern(0xff, time);
	}

	unsigned TTransaction::AddReset()
	{
		const unsigned idx_presence = this->idx_buffer_next + 8U;
		this->AddPattern(0x00, 7U);
		this->AddDelay(4U);
		return idx_presence;
	}

	void TTransaction::AddWrite(const void* const arr_data, const unsigned n_data)
	{
		EL_ERROR(this->idx_buffer_next + n_data * 8U > this->buffer.Count(), TException, "buffer too small");
		Encode(arr_data, n_data, this->idx_buffer_next);
		this->idx_buffer_next += n_data * 8U;
	}

	unsigned TTransaction::AddRead(const unsigned n_data)
	{
		const unsigned idx_read = this->idx_buffer_next;
		EL_ERROR(this->idx_buffer_next + n_data * 8U > this->buffer.Count(), TException, "buffer too small");
		this->AddPattern(SPIBB_ONE, n_data * 8U);
		return idx_read;
	}

	void TTransaction::AddCommand(const u8_t code)
	{
		this->AddWrite(&code, 1U);
	}

	void TTransaction::AddSkipRom()
	{
		this->AddCommand(CMD_SKIP_ROM);
	}

	void TTransaction::AddMatchRom(const uuid_t uuid)
	{
		const rom_t rom = { .uuid = uuid, .crc = CalculateCRC(&uuid, 7U) };
		this->AddCommand(CMD_MATCH_ROM);
		this->AddWrite(&rom, sizeof(rom));
	}

	unsigned TTransaction::AddEnumRom(const uuid_t uuid)
	{
		this->AddCommand(CMD_ENUM_ROM);
		const unsigned pos = this->idx_buffer_next;

		for(unsigned i = 0; i < 56; i++)
		{
			const bool v = uuid.Bit(i);
			this->AddPattern(SPIBB_ONE, 2U);
			this->AddPattern(v ? SPIBB_ONE : SPIBB_ZERO, 1U);
		}

		return pos;
	}

	u8_t TTransaction::ComputeMatchingBits(const unsigned idx_enum_rom, const u8_t idx_start, const bool allow_forks)
	{
		for(u8_t i = idx_start; i < 56; i++)
		{
			const bool has_zero = !BitState(this->buffer[idx_enum_rom + i * 3 + 0]);
			const bool has_one  = !BitState(this->buffer[idx_enum_rom + i * 3 + 1]);
			const bool wanted_bit_value = BitState(this->buffer[idx_enum_rom + i * 3 + 2]);

			if(!allow_forks && has_zero && has_one)
				return i;

			if(wanted_bit_value && !has_one)
				return i;

			if(!wanted_bit_value && !has_zero)
				return i;
		}

		return 56;
	}

	bool TTransaction::EnumRomBitInfo(const unsigned idx_enum_rom, const u8_t idx_bit, bool& has_one, bool& has_zero)
	{
		EL_ERROR(idx_bit > 55, TInvalidArgumentException, "idx_bit", "idx_bit must be smaller than 56");

		has_zero = !BitState(this->buffer[idx_enum_rom + idx_bit * 3 + 0]);
		has_one  = !BitState(this->buffer[idx_enum_rom + idx_bit * 3 + 1]);

		return has_zero || has_one;
	}

	unsigned TTransaction::AddReadRom()
	{
		this->AddCommand(CMD_READ_ROM);
		return this->AddRead(8U);
	}

	void TTransaction::Execute()
	{
		if(this->idx_buffer_next < this->w1bus->min_transfer_bytes)
			this->AddDelay(this->w1bus->min_transfer_bytes - this->idx_buffer_next);
		this->w1bus->ExchangeBuffers(&buffer[0], &buffer[0], this->idx_buffer_next);
	}

	void TTransaction::Clear()
	{
		this->idx_buffer_next = 0;
	}

	TTransaction::TTransaction(TW1BbBus* const w1bus, io::collection::list::array_t<byte_t> buffer) : w1bus(w1bus), buffer(buffer), idx_buffer_next(0)
	{
		EL_ERROR(buffer.Count() < this->w1bus->min_transfer_bytes, TException, "buffer smaller than min_transfer_bytes");
	}

	/**********************************************/

	// void TW1BbBus::SelectDevice(const uuid_t new_uuid, const ESpeed new_speed)
	// {
	// 	if(this->selected_speed != (u8_t)new_speed)
	// 	{
	// 		if(new_speed == ESpeed::OVERDRIVE)
	// 		{
	// 			if(this->selected_uuid != uuid_t::NULL_VALUE)
	// 			{
	// 				// A regular speed device was selected.
	// 				// This means the overdrive capable devices are all disabled,
	// 				// and since communication with regular speed would confuse overdrive devices
	// 				// in overdrive mode, they must have been switched back to regular speed.
	// 				// This means only a full reset can wake them at this moment.
 //
	// 				// full slow reset
	// 				this->Reset(false);
	// 			}
 //
	// 			this->Write(CMD_OVERDRIVE, nullptr, 0);
	// 			this->spidev->Clock(1000000U); // 1MHz
	// 			this->selected_speed = (u8_t)ESpeed::OVERDRIVE;
 //
	// 			// now all non-overdrive capable devices are disabled
	// 			// and all overdrive capable devices are in overdrive mode
	// 		}
	// 		else if(new_speed == ESpeed::REGULAR && this->selected_speed == (u8_t)ESpeed::OVERDRIVE)
	// 		{
	// 			// full slow reset
	// 			this->Reset(false);
 //
	// 			// now all devices are enabled and set to regular speed
	// 		}
	// 	}
 //
	// 	if(this->selected_uuid != new_uuid)
	// 	{
	// 		if(this->selected_uuid != uuid_t::NULL_VALUE)
	// 		{
	// 			if(this->selected_speed == (u8_t)ESpeed::OVERDRIVE && new_speed == ESpeed::OVERDRIVE)
	// 			{
	// 				// overdrive reset
	// 				this->Reset(true);
	// 			}
	// 			else
	// 			{
	// 				// full slow reset
	// 				this->Reset(false);
	// 			}
	// 		}
 //
	// 		this->MatchRom(new_uuid);
	// 	}
	// }

	bool TW1BbBus::Reset()
	{
		this->spidev->Clock(100000U); // 100KHz
		this->selected_speed = (u8_t)ESpeed::REGULAR;

		const unsigned sz_spibuf = util::Max(this->min_transfer_bytes, 11U);
		byte_t spibuf[sz_spibuf];
		memset(spibuf, 0x00, 7);
		memset(spibuf + 7, 0xff, sz_spibuf - 7);

		this->ExchangeBuffers(spibuf, spibuf, sz_spibuf);

		for(unsigned i = 8; i < sz_spibuf; i++)
			if(spibuf[i] != 0xff)
			{
				if(DEBUG) std::cerr<<"DEBUG: TW1BbBus::Reset(): devices detected on the bus\n";
				return true;
			}

		if(DEBUG) std::cerr<<"DEBUG: TW1BbBus::Reset(): *NO* devices detected\n";
		return false;
	}

	void TW1BbBus::ExchangeBuffers(byte_t* const tx_buffer, byte_t* const rx_buffer, const usys_t n_bytes)
	{
		EL_ERROR(n_bytes < this->min_transfer_bytes, TLogicException);

		if(this->invert_tx_polarity != 0 && tx_buffer != nullptr)
			for(usys_t i = 0; i < n_bytes; i++)
				tx_buffer[i] = ~tx_buffer[i];

		while(this->pause_until > TTime::Now(EClock::MONOTONIC))
		{
			TTimeWaitable w_pause(EClock::MONOTONIC, this->pause_until);
			w_pause.WaitFor();
		}

		this->StrongPullup(false);
		this->pause_until = TTime(-1);

		if(DEBUG && tx_buffer) Hexdump(tx_buffer, n_bytes, 16, "TX");
		this->spidev->ExchangeBuffers(tx_buffer, rx_buffer, n_bytes);
		if(DEBUG && rx_buffer) Hexdump(rx_buffer, n_bytes, 16, "RX");
	}

	std::unique_ptr<IW1Device> TW1BbBus::ClaimDevice(const uuid_t uuid)
	{
		return std::unique_ptr<IW1Device>(new TW1BbDevice(this, uuid));
	}

	void TW1BbBus::Scan(TTransaction& tr, io::collection::list::TList<uuid_t>& list, u8_t idx_bit, uuid_t uuid)
	{
		if(DEBUG) std::cerr<<"\nscanning bit "<<(int)idx_bit<<" ...\n";
		if(DEBUG) Hexdump(&uuid, sizeof(uuid), 7U, "scan-uuid");

		bool has_zero;
		bool has_one;

		tr.Clear();
		tr.AddReset();
		const unsigned idx_enum_rom = tr.AddEnumRom(uuid);
		tr.Execute();

		tr.EnumRomBitInfo(idx_enum_rom, idx_bit, has_one, has_zero);
		idx_bit++;

		if(DEBUG) std::cerr<<"has_zero = "<<(has_zero ? "yes" : "no")<<std::endl;
		if(DEBUG) std::cerr<<"has_one  = "<<(has_one  ? "yes" : "no")<<std::endl;

		if(has_zero)
		{
			const u8_t n_match = tr.ComputeMatchingBits(idx_enum_rom, idx_bit, false);
			EL_ERROR(n_match < idx_bit, TLogicException);

			if(idx_bit == 56 || n_match == 56)
			{
				list.Append(uuid);
			}
			else
			{
				Scan(tr, list, n_match, uuid);
			}
		}

		if(has_one)
		{
			uuid.Bit(idx_bit - 1, true);
			if(idx_bit == 56)
			{
				list.Append(uuid);
			}
			else
			{
				Scan(tr, list, idx_bit, uuid);
			}
		}
	}

	TList<uuid_t> TW1BbBus::Scan(const ESpeed speed)
	{
		TList<uuid_t> list;
		byte_t buffer[192];
		TTransaction tr(this, array_t<byte_t>(buffer, sizeof(buffer)));
		Scan(tr, list, 0U, uuid_t::NULL_VALUE);
		return list;
	}

	void TW1BbBus::StrongPullup(const bool state)
	{
		if(this->strong_pullup != nullptr && (state ? 1 : 0) != this->strong_pull_state)
		{
			switch((EStrongPullMode)this->strong_pull_mode)
			{
				case EStrongPullMode::DIRECT_GPIO:
					if(state)
					{
						this->strong_pullup->Mode(EMode::OUTPUT);
						this->strong_pullup->State(true);
					}
					else
					{
						this->strong_pullup->Mode(EMode::DISABLED);
					}
					this->strong_pullup->Commit();
					break;

				case EStrongPullMode::P_MOSFET:
					this->strong_pullup->State(!state);
					break;

				case EStrongPullMode::MISO:
					if(state)
					{
						this->strong_pullup->Mode(EMode::OUTPUT);
						this->strong_pullup->State(true);
					}
					else
					{
						this->strong_pullup->AlternateFunction(this->miso_altfunc);
					}
					this->strong_pullup->Commit();
					break;
			}

			this->strong_pull_state = state ? 1 : 0;
		}
	}

	void TW1BbBus::PauseBus(const TTime duration)
	{
		const TTime pause_until_new = TTime::Now(EClock::MONOTONIC) + duration;
		if(this->pause_until < pause_until_new)
			this->pause_until = pause_until_new;

		this->StrongPullup(true);
	}

	TTime TW1BbBus::PausedUntil() const
	{
		return this->pause_until;
	}

	TW1BbBus::TW1BbBus(std::unique_ptr<spi::ISpiDevice> spidev, const bool invert_tx_polarity, const ESpeed max_allowed_speed, const u8_t min_transfer_bytes) :
		spidev(std::move(spidev)),
		pause_until(-1),
		min_transfer_bytes(min_transfer_bytes),
		miso_altfunc(-1),
		invert_tx_polarity(invert_tx_polarity ? 1 : 0),
		max_allowed_speed((u8_t)max_allowed_speed),
		strong_pull_mode(0),
		strong_pull_state(0),
		selected_speed((u8_t)ESpeed::REGULAR)
	{
		this->spidev->Clock(100000U); // 100KHz
	}

	TW1BbBus::TW1BbBus(std::unique_ptr<spi::ISpiDevice> spidev, const bool invert_tx_polarity, const ESpeed max_allowed_speed, std::unique_ptr<gpio::IPin> strong_pullup, const EStrongPullMode strong_pull_mode, const int miso_altfunc, const u8_t min_transfer_bytes) :
		spidev(std::move(spidev)),
		strong_pullup(std::move(strong_pullup)),
		pause_until(-1),
		min_transfer_bytes(min_transfer_bytes),
		miso_altfunc(miso_altfunc),
		invert_tx_polarity(invert_tx_polarity ? 1 : 0),
		max_allowed_speed((u8_t)max_allowed_speed),
		strong_pull_mode((u8_t)strong_pull_mode),
		strong_pull_state(0),
		selected_speed((u8_t)ESpeed::REGULAR)
	{
		switch((EStrongPullMode)this->strong_pull_mode)
		{
			case EStrongPullMode::DIRECT_GPIO:
				this->strong_pullup->AutoCommit(false);
				this->strong_pullup->Mode(EMode::DISABLED);
				this->strong_pullup->Commit();
				break;
			case EStrongPullMode::P_MOSFET:
				this->strong_pullup->AutoCommit(true);
				this->strong_pullup->Mode(EMode::OUTPUT);
				this->strong_pullup->State(true);
				break;
			case EStrongPullMode::MISO:
				this->strong_pullup->AutoCommit(false);
				this->strong_pullup->AlternateFunction(this->miso_altfunc);
				this->strong_pullup->Commit();
				break;
		}

		this->spidev->Clock(100000U); // 100KHz
	}

	TW1BbBus::~TW1BbBus()
	{
		// this->Reset(false);
	}
}
