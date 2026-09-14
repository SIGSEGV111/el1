#include "io_net_bluetooth.hpp"
#include "error.hpp"
#include <cstdio>
#include <cstring>

namespace el1::io::net::bluetooth
{
	using namespace error;
	using namespace io::collection::list;
	using namespace text::string;

	namespace
	{
		constexpr byte_t BLUETOOTH_UUID_BASE_TAIL[12] =
		{
			0x00, 0x00, 0x10, 0x00,
			0x80, 0x00, 0x00, 0x80,
			0x5f, 0x9b, 0x34, 0xfb,
		};

		bool uuidHasBluetoothBaseTail(const TUuid& uuid)
		{
			return ::memcmp(&uuid.octet[4], BLUETOOTH_UUID_BASE_TAIL, sizeof(BLUETOOTH_UUID_BASE_TAIL)) == 0;
		}
	}

	TUuid::TUuid() : octet{}
	{
	}

	TUuid::TUuid(const u16_t bluetooth_assigned_number) :
		TUuid(static_cast<u32_t>(bluetooth_assigned_number))
	{
	}

	TUuid::TUuid(const u32_t bluetooth_assigned_number) : octet
	{
		static_cast<byte_t>((bluetooth_assigned_number >> 24U) & 0xffU),
		static_cast<byte_t>((bluetooth_assigned_number >> 16U) & 0xffU),
		static_cast<byte_t>((bluetooth_assigned_number >>  8U) & 0xffU),
		static_cast<byte_t>((bluetooth_assigned_number >>  0U) & 0xffU),
		BLUETOOTH_UUID_BASE_TAIL[0], BLUETOOTH_UUID_BASE_TAIL[1], BLUETOOTH_UUID_BASE_TAIL[2], BLUETOOTH_UUID_BASE_TAIL[3],
		BLUETOOTH_UUID_BASE_TAIL[4], BLUETOOTH_UUID_BASE_TAIL[5], BLUETOOTH_UUID_BASE_TAIL[6], BLUETOOTH_UUID_BASE_TAIL[7],
		BLUETOOTH_UUID_BASE_TAIL[8], BLUETOOTH_UUID_BASE_TAIL[9], BLUETOOTH_UUID_BASE_TAIL[10], BLUETOOTH_UUID_BASE_TAIL[11],
	}
	{
	}

	TUuid::TUuid(const TStringView uuid) : octet{}
	{
		const std::unique_ptr<char[]> text = uuid.MakeCStr();
		u32_t values[16] = {};
		int consumed = 0;

		if(uuid.Length() == 4)
		{
			u32_t value = 0;
			const int count = ::sscanf(text.get(), "%4x%n", &value, &consumed);
			EL_ERROR(count != 1 || consumed != 4, TInvalidArgumentException, "uuid", "must be a 16/32/128-bit Bluetooth UUID");
			*this = TUuid(static_cast<u16_t>(value));
			return;
		}

		if(uuid.Length() == 8)
		{
			u32_t value = 0;
			const int count = ::sscanf(text.get(), "%8x%n", &value, &consumed);
			EL_ERROR(count != 1 || consumed != 8, TInvalidArgumentException, "uuid", "must be a 16/32/128-bit Bluetooth UUID");
			*this = TUuid(value);
			return;
		}

		const int count = ::sscanf(
			text.get(),
			"%2x%2x%2x%2x-%2x%2x-%2x%2x-%2x%2x-%2x%2x%2x%2x%2x%2x%n",
			&values[0], &values[1], &values[2], &values[3],
			&values[4], &values[5], &values[6], &values[7],
			&values[8], &values[9], &values[10], &values[11],
			&values[12], &values[13], &values[14], &values[15],
			&consumed
		);
		EL_ERROR(count != 16 || consumed != static_cast<int>(uuid.Length()), TInvalidArgumentException, "uuid", "must be a 16/32/128-bit Bluetooth UUID");
		for(usys_t i = 0; i < 16; i++)
			octet[i] = static_cast<byte_t>(values[i]);
	}

	TUuid::operator TString() const
	{
		return TString::Format(
			U"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
			octet[0], octet[1], octet[2], octet[3],
			octet[4], octet[5], octet[6], octet[7],
			octet[8], octet[9], octet[10], octet[11],
			octet[12], octet[13], octet[14], octet[15]
		);
	}

	std::optional<u32_t> TUuid::BluetoothAssignedNumber32() const
	{
		if(!uuidHasBluetoothBaseTail(*this))
			return std::nullopt;
		return
			(static_cast<u32_t>(octet[0]) << 24U) |
			(static_cast<u32_t>(octet[1]) << 16U) |
			(static_cast<u32_t>(octet[2]) <<  8U) |
			(static_cast<u32_t>(octet[3]) <<  0U);
	}

	std::optional<u16_t> TUuid::BluetoothAssignedNumber16() const
	{
		const std::optional<u32_t> value = BluetoothAssignedNumber32();
		if(!value.has_value() || *value > 0xffffU)
			return std::nullopt;
		return static_cast<u16_t>(*value);
	}

	bool TService::HasServiceClass(const TUuid& uuid) const
	{
		for(const TUuid& candidate : service_class_uuids)
			if(candidate == uuid)
				return true;
		return false;
	}

	bool TService::HasProfile(const TUuid& uuid) const
	{
		for(const TProfileDescriptor& candidate : profiles)
			if(candidate.uuid == uuid)
				return true;
		return false;
	}

	bool TService::HasProtocol(const TUuid& uuid) const
	{
		return FindProtocol(uuid) != nullptr;
	}

	const TProtocolDescriptor* TService::FindProtocol(const TUuid& uuid) const
	{
		for(const TProtocolDescriptor& candidate : protocols)
			if(candidate.uuid == uuid)
				return &candidate;
		return nullptr;
	}

	std::optional<u8_t> TService::RfcommChannel() const
	{
		const TProtocolDescriptor* const protocol = FindProtocol(UUID_PROTOCOL_RFCOMM);
		if(protocol == nullptr || protocol->numeric_parameters.Count() == 0 || protocol->numeric_parameters[0] == 0 || protocol->numeric_parameters[0] > 30)
			return std::nullopt;
		return static_cast<u8_t>(protocol->numeric_parameters[0]);
	}

	std::optional<u16_t> TService::L2capPsm() const
	{
		const TProtocolDescriptor* const protocol = FindProtocol(UUID_PROTOCOL_L2CAP);
		if(protocol == nullptr || protocol->numeric_parameters.Count() == 0 || protocol->numeric_parameters[0] == 0 || protocol->numeric_parameters[0] > 0xffffU)
			return std::nullopt;
		return static_cast<u16_t>(protocol->numeric_parameters[0]);
	}

	bool TServiceDiscoveryFilter::Matches(const TService& service) const
	{
		for(const TUuid& uuid : service_class_uuids)
			if(!service.HasServiceClass(uuid))
				return false;

		for(const TUuid& uuid : profile_uuids)
			if(!service.HasProfile(uuid))
				return false;

		for(const TUuid& uuid : protocol_uuids)
			if(!service.HasProtocol(uuid))
				return false;

		switch(name_match)
		{
			case EServiceNameMatch::ANY:
				break;
			case EServiceNameMatch::EXACT:
				if(service.name != name)
					return false;
				break;
			case EServiceNameMatch::PREFIX:
				if(!service.name.BeginsWith(name))
					return false;
				break;
			case EServiceNameMatch::CONTAINS:
				if(!service.name.Contains(name))
					return false;
				break;
		}

		return true;
	}

	address_t::address_t() : octet{}
	{
	}

	address_t::address_t(const TStringView address) : octet{}
	{
		auto c_str = address.MakeCStr();
		u32_t parsed[6] = {};
		int consumed = 0;
		const int count = ::sscanf(
			c_str.get(),
			"%2x:%2x:%2x:%2x:%2x:%2x%n",
			&parsed[0], &parsed[1], &parsed[2], &parsed[3], &parsed[4], &parsed[5], &consumed
		);
		EL_ERROR(count != 6 || consumed != static_cast<int>(::strlen(c_str.get())), TInvalidArgumentException, "address", "must use XX:XX:XX:XX:XX:XX format");
		for(usys_t i = 0; i < 6; i++)
			octet[i] = static_cast<byte_t>(parsed[i]);
	}

	address_t::operator TString() const
	{
		return TString::Format(
			U"%02x:%02x:%02x:%02x:%02x:%02x",
			octet[0], octet[1], octet[2], octet[3], octet[4], octet[5]
		);
	}
}
