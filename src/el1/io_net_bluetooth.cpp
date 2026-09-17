#include "io_net_bluetooth.hpp"
#include "error.hpp"
#include "io_text_number.hpp"
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
		if(uuid.Length() == 4)
		{
			const auto value = text::number::TryParseHex<u16_t>(uuid);
			EL_ERROR(!value.has_value(), TInvalidArgumentException, "uuid", "must be a 16/32/128-bit Bluetooth UUID");
			*this = TUuid(*value);
			return;
		}

		if(uuid.Length() == 8)
		{
			const auto value = text::number::TryParseHex<u32_t>(uuid);
			EL_ERROR(!value.has_value(), TInvalidArgumentException, "uuid", "must be a 16/32/128-bit Bluetooth UUID");
			*this = TUuid(*value);
			return;
		}

		static constexpr usys_t OCTET_POSITIONS[16] = { 0, 2, 4, 6, 9, 11, 14, 16, 19, 21, 24, 26, 28, 30, 32, 34 };
		EL_ERROR(uuid.Length() != 36 || uuid[8] != U'-' || uuid[13] != U'-' || uuid[18] != U'-' || uuid[23] != U'-',
			TInvalidArgumentException, "uuid", "must be a 16/32/128-bit Bluetooth UUID");

		for(usys_t i = 0; i < 16; i++)
		{
			const auto value = text::number::TryParseHex<byte_t>(uuid.SliceSL(OCTET_POSITIONS[i], 2));
			EL_ERROR(!value.has_value(), TInvalidArgumentException, "uuid", "must be a 16/32/128-bit Bluetooth UUID");
			octet[i] = *value;
		}
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
		EL_ERROR(address.Length() != 17 || address[2] != U':' || address[5] != U':' || address[8] != U':' ||
			address[11] != U':' || address[14] != U':', TInvalidArgumentException, "address", "must use XX:XX:XX:XX:XX:XX format");

		for(usys_t i = 0; i < 6; i++)
		{
			const auto value = text::number::TryParseHex<byte_t>(address.SliceSL(i * 3, 2));
			EL_ERROR(!value.has_value(), TInvalidArgumentException, "address", "must use XX:XX:XX:XX:XX:XX format");
			octet[i] = *value;
		}
	}

	address_t::operator TString() const
	{
		return TString::Format(
			U"%02x:%02x:%02x:%02x:%02x:%02x",
			octet[0], octet[1], octet[2], octet[3], octet[4], octet[5]
		);
	}
}
