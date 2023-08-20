#pragma once

#include "def.hpp"
#include "dev_i2c.hpp"
#include "io_stream.hpp"
#include "io_collection_map.hpp"
#include "io_file.hpp"

namespace el1::dev::i2c::native
{
	class TBus;
	class TDevice;

	class TDevice : public II2CDevice
	{
		friend class TBus;
		protected:
			io::stream::TKernelStream stream;
			TBus* const bus;
			const u8_t address;
			const ESpeedClass sc;

			TDevice(TBus* const bus, const u8_t address, const ESpeedClass sc);
			~TDevice();

		public:
			usys_t Read(byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			const system::waitable::IWaitable* OnInputReady() const final override;
			iosize_t WriteOut(ISink<byte_t>& sink, const iosize_t n_items_max = (iosize_t)-1, const bool allow_recursion = true) final override;
			usys_t BlockingRead(byte_t* const arr_items, const usys_t n_items_max, system::time::TTime timeout = -1, const bool absolute_time = false) final override EL_WARN_UNUSED_RESULT;
			void ReadAll(byte_t* const arr_items, const usys_t n_items) final override;
			usys_t Write(const byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			const system::waitable::IWaitable* OnOutputReady() const final override;
			iosize_t ReadIn(ISource<byte_t>& source, const iosize_t n_items_max = (iosize_t)-1, const bool allow_recursion = true) final override;
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
