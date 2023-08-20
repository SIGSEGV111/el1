#pragma once

#include "io_types.hpp"
#include "io_stream.hpp"
#include "system_time.hpp"
#include "error.hpp"

namespace io::text::string
{
	class TString;
}

namespace el1::dev::w1
{
	using namespace io::types;

	struct IW1Bus;
	struct IW1Device;

	enum class EPowerSource
	{
		AUTO_DETECT,	// for this to work the VCC-pin (if present) usually must be connected to GND (read the datasheet!)
		PARASITIC,
		DEDICATED,
	};

	struct serial_t
	{
		byte_t octet[6];

		bool operator==(const serial_t&) const = default;
		bool operator!=(const serial_t&) const = default;
	};

	struct uuid_t
	{
		u8_t type;
		serial_t serial;

		bool operator==(const uuid_t&) const = default;
		bool operator!=(const uuid_t&) const = default;

		byte_t& Octet(const unsigned idx_byte) EL_GETTER;
		const byte_t& Octet(const unsigned idx_byte) const EL_GETTER;

		void Bit(const unsigned idx_bit, const bool new_state) EL_SETTER;
		bool Bit(const unsigned idx_bit) const EL_GETTER;

		io::text::string::TString ToString() const EL_GETTER;

		static const uuid_t NULL_VALUE;

		static uuid_t FromInt(const u64_t);
	};

	struct rom_t
	{
		uuid_t uuid;
		byte_t crc;

		bool operator==(const rom_t&) const = default;
		bool operator!=(const rom_t&) const = default;
	};

	struct TCrcMismatchException : error::IException
	{
		const uuid_t uuid;
		const u8_t received_crc;
		const u8_t calculated_crc;

		io::text::string::TString Message() const final override;
		error::IException* Clone() const override;

		TCrcMismatchException(const uuid_t uuid, const u8_t received_crc, const u8_t calculated_crc) : uuid(uuid), received_crc(received_crc), calculated_crc(calculated_crc)
		{
		}
	};

	enum class ESpeed : u8_t
	{
		REGULAR = 0,
		OVERDRIVE = 1
	};

	static const u8_t CMD_SKIP_ROM  = 0xCC;
	static const u8_t CMD_MATCH_ROM = 0x55;
	static const u8_t CMD_ENUM_ROM  = 0xF0;
	static const u8_t CMD_READ_ROM  = 0x33;
	static const u8_t CMD_OVERDRIVE = 0x3C;

	struct IW1Device
	{
		virtual IW1Bus* Bus() const EL_GETTER = 0;
		virtual uuid_t UUID() const EL_GETTER = 0;
		virtual ESpeed Speed() const EL_GETTER = 0;
		virtual void Speed(const ESpeed) EL_SETTER = 0;
		virtual void Read(const u8_t cmd, void* const buffer, const u8_t n_bytes) = 0;
		virtual void Write(const u8_t cmd, const void* const buffer, const u8_t n_bytes) = 0;
		virtual ~IW1Device() {}
	};

	struct IW1Bus
	{
		virtual bool HasStrongPullUp() const EL_GETTER = 0;

		// Returns true if devices were detected on the bus after the reset, false otherwise.
		// All bus implementations ensure the bus is Reset() during initialization and shutdown.
		// You only need to call this function to recover from a error.
		virtual bool Reset() = 0;

		virtual std::unique_ptr<IW1Device> ClaimDevice(const uuid_t uuid) = 0;

		// only devices which support the requested speed are detected
		virtual io::collection::list::TList<uuid_t> Scan(const ESpeed speed = ESpeed::REGULAR) = 0;

		// Suspends bus activity for the specified duration.
		// This is supposed to be used by drivers if their hardware needs to draw power from the bus.
		// Any function that needs to send or receive data on this bus will block until the end of the pause.
		// If multiple drivers call PauseBus(), the pause is extended to compensate for the longest duration.
		// If the bus has a "strong pull-up" feature, it will be activated during the pause.
		virtual void PauseBus(const system::time::TTime duration) = 0;

		// returns the absolute time measured against EClock::MONOTONIC when the current pause will expire
		// returns TTime(-1) if no pause is active at the moment
		virtual system::time::TTime PausedUntil() const EL_GETTER = 0;
	};

	u8_t CalculateCRC(const void* const arr_data, const unsigned n_data);
}
