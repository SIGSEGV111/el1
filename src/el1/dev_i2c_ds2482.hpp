#pragma once

#include "dev_i2c.hpp"
#include "dev_w1.hpp"
#include "io_collection_list.hpp"
#include "system_task.hpp"
#include "system_time.hpp"

namespace el1::dev::i2c::ds2482
{
	using namespace el1::dev::w1;
	using namespace el1::io::collection::list;
	using namespace el1::io::types;
	using namespace el1::system::task;
	using namespace el1::system::time;

	class TDS2482Bus;

	class TDS2482Device final : public IW1Device
	{
		private:
			TDS2482Bus* const bus;
			const uuid_t uuid;
			ESpeed speed;

		public:
			IW1Bus* Bus() const final override EL_GETTER;
			uuid_t UUID() const final override EL_GETTER;
			ESpeed Speed() const final override EL_GETTER;
			void Speed(const ESpeed new_speed) final override EL_SETTER;
			void Read(const u8_t cmd, void* const buffer, const u8_t n_bytes) final override;
			void Write(const u8_t cmd, const void* const buffer, const u8_t n_bytes) final override;
			void WritePowered(const u8_t cmd, const void* const buffer, const u8_t n_bytes, const TTime duration) final override;

			TDS2482Device(TDS2482Bus* const bus, const uuid_t uuid);
			~TDS2482Device();
	};

	class TDS2482Bus final : public IW1Bus
	{
		friend class TDS2482Device;

		private:
			static const u8_t CMD_DEVICE_RESET = 0xF0;
			static const u8_t CMD_SET_READ_POINTER = 0xE1;
			static const u8_t CMD_WRITE_CONFIGURATION = 0xD2;
			static const u8_t CMD_W1_RESET = 0xB4;
			static const u8_t CMD_W1_WRITE_BYTE = 0xA5;
			static const u8_t CMD_W1_READ_BYTE = 0x96;
			static const u8_t CMD_W1_TRIPLET = 0x78;

			static const u8_t POINTER_STATUS = 0xF0;
			static const u8_t POINTER_READ_DATA = 0xE1;
			static const u8_t POINTER_CONFIGURATION = 0xC3;

			static const u8_t STATUS_BUSY = 0x01;
			static const u8_t STATUS_PRESENCE = 0x02;
			static const u8_t STATUS_SHORT = 0x04;
			static const u8_t STATUS_RESET = 0x10;
			static const u8_t STATUS_SINGLE_BIT = 0x20;
			static const u8_t STATUS_TRIPLET_SECOND = 0x40;
			static const u8_t STATUS_DIRECTION = 0x80;

			static const u8_t CONFIG_ACTIVE_PULLUP = 0x01;
			static const u8_t CONFIG_STRONG_PULLUP = 0x04;

			std::unique_ptr<II2CDevice> device;
			mutable TFiberMutex mutex;
			TList<TDS2482Device*> claimed_devices;
			TTime pause_until;
			const bool active_pullup;
			u8_t configuration;

			void waitPause();
			void writeCommand(const u8_t command);
			void writeCommand(const u8_t command, const u8_t parameter);
			u8_t readPointer(const u8_t pointer);
			u8_t waitBusy(const TTime timeout = TTime(0.02));
			void writeConfiguration(const u8_t new_configuration);
			void prepare(const ESpeed speed, const bool strong_pullup = false);
			bool resetUnlocked();
			void writeByteUnlocked(const u8_t value);
			u8_t readByteUnlocked();
			u8_t tripletUnlocked(const bool direction);
			void matchRomUnlocked(const uuid_t uuid);
			void transactRead(const uuid_t uuid, const ESpeed speed, const u8_t cmd, void* const buffer, const u8_t n_bytes);
			void transactWrite(const uuid_t uuid, const ESpeed speed, const u8_t cmd, const void* const buffer, const u8_t n_bytes, const TTime powered_duration);

		public:
			bool HasStrongPullUp() const final override EL_GETTER;
			bool Reset() final override;
			std::unique_ptr<IW1Device> ClaimDevice(const uuid_t uuid) final override;
			TList<uuid_t> Scan(const ESpeed speed = ESpeed::REGULAR) final override;
			void PauseBus(const TTime duration) final override;
			TTime PausedUntil() const final override EL_GETTER;

			TDS2482Bus(std::unique_ptr<II2CDevice> device, const bool active_pullup = true);
			~TDS2482Bus();
	};
}
