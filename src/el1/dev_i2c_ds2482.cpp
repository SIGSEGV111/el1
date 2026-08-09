#include "dev_i2c_ds2482.hpp"
#include "error.hpp"
#include "io_text_string.hpp"

namespace el1::dev::i2c::ds2482
{
	using namespace el1::error;
	using namespace el1::io::text::string;

	IW1Bus* TDS2482Device::Bus() const
	{
		return bus;
	}

	uuid_t TDS2482Device::UUID() const
	{
		return uuid;
	}

	ESpeed TDS2482Device::Speed() const
	{
		return speed;
	}

	void TDS2482Device::Speed(const ESpeed new_speed)
	{
		EL_ERROR(new_speed != ESpeed::REGULAR, TInvalidArgumentException, "new_speed", "DS2482 overdrive device selection is not implemented");
		speed = new_speed;
	}

	void TDS2482Device::Read(const u8_t cmd, void* const buffer, const u8_t n_bytes)
	{
		bus->transactRead(uuid, speed, cmd, buffer, n_bytes);
	}

	void TDS2482Device::Write(const u8_t cmd, const void* const buffer, const u8_t n_bytes)
	{
		bus->transactWrite(uuid, speed, cmd, buffer, n_bytes, TTime(0));
	}

	void TDS2482Device::WritePowered(const u8_t cmd, const void* const buffer, const u8_t n_bytes, const TTime duration)
	{
		EL_ERROR(duration <= TTime(0), TInvalidArgumentException, "duration", "must be greater than zero");
		bus->transactWrite(uuid, speed, cmd, buffer, n_bytes, duration);
	}

	TDS2482Device::TDS2482Device(TDS2482Bus* const bus, const uuid_t uuid) :
		bus(bus),
		uuid(uuid),
		speed(ESpeed::REGULAR)
	{
		EL_ERROR(bus == nullptr, TInvalidArgumentException, "bus", "must not be null");
		const TMutexAutoLock lock(&bus->mutex);
		for(const TDS2482Device* const claimed : bus->claimed_devices)
			EL_ERROR(claimed->uuid == uuid, TException, TString::Format(U"1-wire device already claimed: %s", uuid.ToString()));
		bus->claimed_devices.Append(this);
	}

	TDS2482Device::~TDS2482Device()
	{
		const TMutexAutoLock lock(&bus->mutex);
		bus->claimed_devices.RemoveItem(this, 1);
	}

	void TDS2482Bus::waitPause()
	{
		const TTime now = TTime::Now(EClock::MONOTONIC);
		if(pause_until > now)
			TFiber::Sleep(pause_until - now);
		pause_until = TTime(-1);
	}

	void TDS2482Bus::writeCommand(const u8_t command)
	{
		device->WriteAll(&command, 1);
	}

	void TDS2482Bus::writeCommand(const u8_t command, const u8_t parameter)
	{
		const byte_t data[2] = { command, parameter };
		device->WriteAll(data, 2);
	}

	u8_t TDS2482Bus::readPointer(const u8_t pointer)
	{
		writeCommand(CMD_SET_READ_POINTER, pointer);
		u8_t value;
		device->ReadAll(&value, 1);
		return value;
	}

	u8_t TDS2482Bus::waitBusy(const TTime timeout)
	{
		const TTime deadline = TTime::Now(EClock::MONOTONIC) + timeout;
		u8_t status;
		do
		{
			device->ReadAll(&status, 1);
			if((status & STATUS_BUSY) == 0)
				return status;
			TFiber::Sleep(TTime(0.00005));
		}
		while(TTime::Now(EClock::MONOTONIC) < deadline);
		EL_THROW(TException, "DS2482 timed out waiting for 1-wire operation");
	}

	void TDS2482Bus::writeConfiguration(const u8_t new_configuration)
	{
		const u8_t low_nibble = new_configuration & 0x0F;
		const u8_t encoded = low_nibble | static_cast<u8_t>((~low_nibble & 0x0F) << 4);
		writeCommand(CMD_WRITE_CONFIGURATION, encoded);
		const u8_t verified = readPointer(POINTER_CONFIGURATION);
		EL_ERROR(verified != low_nibble, TException, TString::Format(U"DS2482 rejected configuration 0x%02x (read back 0x%02x)", low_nibble, verified));
		configuration = low_nibble;
	}

	void TDS2482Bus::prepare(const ESpeed speed, const bool strong_pullup)
	{
		EL_ERROR(speed != ESpeed::REGULAR, TInvalidArgumentException, "speed", "DS2482 overdrive device selection is not implemented");
		waitPause();
		u8_t wanted = active_pullup ? CONFIG_ACTIVE_PULLUP : 0;
		if(strong_pullup)
			wanted |= CONFIG_STRONG_PULLUP;
		if(configuration != wanted)
			writeConfiguration(wanted);
	}

	bool TDS2482Bus::resetUnlocked()
	{
		writeCommand(CMD_W1_RESET);
		const u8_t status = waitBusy();
		EL_ERROR((status & STATUS_SHORT) != 0, TException, "DS2482 detected a short circuit on the 1-wire bus");
		return (status & STATUS_PRESENCE) != 0;
	}

	void TDS2482Bus::writeByteUnlocked(const u8_t value)
	{
		writeCommand(CMD_W1_WRITE_BYTE, value);
		waitBusy();
	}

	u8_t TDS2482Bus::readByteUnlocked()
	{
		writeCommand(CMD_W1_READ_BYTE);
		waitBusy();
		return readPointer(POINTER_READ_DATA);
	}

	u8_t TDS2482Bus::tripletUnlocked(const bool direction)
	{
		writeCommand(CMD_W1_TRIPLET, direction ? 0x80 : 0x00);
		return waitBusy();
	}

	void TDS2482Bus::matchRomUnlocked(const uuid_t uuid)
	{
		const rom_t rom = { .uuid = uuid, .crc = CalculateCRC(&uuid, sizeof(uuid)) };
		writeByteUnlocked(CMD_MATCH_ROM);
		for(usys_t i = 0; i < sizeof(rom); i++)
			writeByteUnlocked(reinterpret_cast<const byte_t*>(&rom)[i]);
	}

	void TDS2482Bus::transactRead(const uuid_t uuid, const ESpeed speed, const u8_t cmd, void* const buffer, const u8_t n_bytes)
	{
		const TMutexAutoLock lock(&mutex);
		prepare(speed);
		EL_ERROR(!resetUnlocked(), TException, TString::Format(U"1-wire device not present: %s", uuid.ToString()));
		matchRomUnlocked(uuid);
		writeByteUnlocked(cmd);
		for(usys_t i = 0; i < n_bytes; i++)
			reinterpret_cast<byte_t*>(buffer)[i] = readByteUnlocked();
	}

	void TDS2482Bus::transactWrite(const uuid_t uuid, const ESpeed speed, const u8_t cmd, const void* const buffer, const u8_t n_bytes, const TTime powered_duration)
	{
		const TMutexAutoLock lock(&mutex);
		prepare(speed);
		EL_ERROR(!resetUnlocked(), TException, TString::Format(U"1-wire device not present: %s", uuid.ToString()));
		matchRomUnlocked(uuid);

		const byte_t* const bytes = reinterpret_cast<const byte_t*>(buffer);
		if(powered_duration > TTime(0) && n_bytes == 0)
		{
			prepare(speed, true);
			writeByteUnlocked(cmd);
		}
		else
		{
			writeByteUnlocked(cmd);
			for(usys_t i = 0; i < n_bytes; i++)
			{
				if(powered_duration > TTime(0) && i + 1 == n_bytes)
					prepare(speed, true);
				writeByteUnlocked(bytes[i]);
			}
		}

		if(powered_duration > TTime(0))
			pause_until = TTime::Now(EClock::MONOTONIC) + powered_duration;
	}

	bool TDS2482Bus::HasStrongPullUp() const
	{
		return true;
	}

	bool TDS2482Bus::Reset()
	{
		const TMutexAutoLock lock(&mutex);
		prepare(ESpeed::REGULAR);
		return resetUnlocked();
	}

	std::unique_ptr<IW1Device> TDS2482Bus::ClaimDevice(const uuid_t uuid)
	{
		return New<TDS2482Device, IW1Device>(this, uuid);
	}

	TList<uuid_t> TDS2482Bus::Scan(const ESpeed speed)
	{
		EL_ERROR(speed != ESpeed::REGULAR, TInvalidArgumentException, "speed", "DS2482 overdrive scan is not implemented");
		const TMutexAutoLock lock(&mutex);
		TList<uuid_t> result;
		rom_t rom = {};
		u8_t last_discrepancy = 0;
		bool last_device = false;

		while(!last_device)
		{
			prepare(speed);
			if(!resetUnlocked())
				break;
			writeByteUnlocked(CMD_ENUM_ROM);

			u8_t last_zero = 0;
			bool search_failed = false;
			for(u8_t bit_number = 1; bit_number <= 64; bit_number++)
			{
				const u8_t byte_index = static_cast<u8_t>((bit_number - 1) / 8);
				const u8_t bit_mask = static_cast<u8_t>(1U << ((bit_number - 1) % 8));
				bool direction;
				if(bit_number < last_discrepancy)
					direction = (reinterpret_cast<byte_t*>(&rom)[byte_index] & bit_mask) != 0;
				else
					direction = bit_number == last_discrepancy;

				const u8_t status = tripletUnlocked(direction);
				const bool id_bit = (status & STATUS_SINGLE_BIT) != 0;
				const bool complement_bit = (status & STATUS_TRIPLET_SECOND) != 0;
				if(id_bit && complement_bit)
				{
					search_failed = true;
					break;
				}

				const bool selected = (status & STATUS_DIRECTION) != 0;
				if(!id_bit && !complement_bit && !selected)
					last_zero = bit_number;
				if(selected)
					reinterpret_cast<byte_t*>(&rom)[byte_index] |= bit_mask;
				else
					reinterpret_cast<byte_t*>(&rom)[byte_index] &= static_cast<u8_t>(~bit_mask);
			}

			if(search_failed)
				break;
			const u8_t crc = CalculateCRC(&rom, sizeof(rom) - 1);
			EL_ERROR(crc != rom.crc, TCrcMismatchException, rom.uuid, rom.crc, crc);
			result.Append(rom.uuid);
			last_discrepancy = last_zero;
			last_device = last_discrepancy == 0;
		}

		return result;
	}

	void TDS2482Bus::PauseBus(const TTime duration)
	{
		EL_ERROR(duration < TTime(0), TInvalidArgumentException, "duration", "must not be negative");
		const TMutexAutoLock lock(&mutex);
		const TTime requested = TTime::Now(EClock::MONOTONIC) + duration;
		if(requested > pause_until)
			pause_until = requested;
	}

	TTime TDS2482Bus::PausedUntil() const
	{
		const TMutexAutoLock lock(&mutex);
		if(pause_until <= TTime::Now(EClock::MONOTONIC))
			return TTime(-1);
		return pause_until;
	}

	TDS2482Bus::TDS2482Bus(std::unique_ptr<II2CDevice> device, const bool active_pullup) :
		device(std::move(device)),
		pause_until(-1),
		active_pullup(active_pullup),
		configuration(0xFF)
	{
		EL_ERROR(this->device == nullptr, TInvalidArgumentException, "device", "I2C device must not be null");
		writeCommand(CMD_DEVICE_RESET);
		const u8_t status = readPointer(POINTER_STATUS);
		EL_ERROR((status & STATUS_RESET) == 0, TException, TString::Format(U"DS2482 reset verification failed, status=0x%02x", status));
		configuration = 0;
		writeConfiguration(active_pullup ? CONFIG_ACTIVE_PULLUP : 0);
		resetUnlocked();
	}

	TDS2482Bus::~TDS2482Bus()
	{
		try
		{
			writeCommand(CMD_DEVICE_RESET);
		}
		catch(...)
		{
		}
	}
}
