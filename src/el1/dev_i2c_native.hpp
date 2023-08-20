#pragma once

#include "def.hpp"
#include "dev_i2c.hpp"
#include "system_handle.hpp"
#include "io_collection_map.hpp"
#include "system_task.hpp"
#include "io_file.hpp"

namespace el1::dev::i2c::native
{
	class TBus;
	class TDevice;

	class TDevice : public II2CDevice
	{
		friend class TBus;
		protected:
			system::handle::THandle handle;
			TBus* const bus;
			const u8_t address;
			const ESpeedClass sc;

			TDevice(TBus* const bus, const u8_t address, const ESpeedClass sc);
			~TDevice();

		public:
			usys_t Read(byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			usys_t Write(const byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			void Flush() final override;

			II2CBus* Bus() const final override EL_GETTER;
			u8_t Address() const final override EL_GETTER;
			ESpeedClass SpeedClass() const final override EL_GETTER;
	};

	class TBus : public II2CBus
	{
		friend class TDevice;
		protected:
			const io::file::TPath device;
			io::collection::map::TSortedMap<u8_t, TDevice*> claimed_addresses;

		public:
			ESpeedClass MaxSupportedSpeed() const final override EL_GETTER;
			std::unique_ptr<II2CDevice> ClaimDevice(const u8_t address, const ESpeedClass sc_max = ESpeedClass::STD) final override;

			TBus(io::file::TPath device);
	};
}
