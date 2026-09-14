#include <gtest/gtest.h>
#include <el1/error.hpp>
#include <el1/io_net_bluetooth.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::error;
	using namespace el1::io::collection::list;
	using namespace el1::io::net::bluetooth;
	using namespace el1::io::text::string;

	TEST(io_net_bluetooth_address_t, ParseAndFormat)
	{
		const address_t address(U"AA:bb:01:02:FE:ff");
		EXPECT_EQ(static_cast<TString>(address), U"aa:bb:01:02:fe:ff");
		EXPECT_EQ(address, address_t(U"aa:bb:01:02:fe:ff"));
		EXPECT_THROW(address_t(U"aa:bb:cc"), TInvalidArgumentException);
		EXPECT_THROW(address_t(U"gg:bb:01:02:03:04"), TInvalidArgumentException);
	}

	TEST(io_net_bluetooth_uuid, ParseFormatAndAssignedNumbers)
	{
		const TUuid serial_port(0x1101U);
		EXPECT_EQ(static_cast<TString>(serial_port), U"00001101-0000-1000-8000-00805f9b34fb");
		ASSERT_TRUE(serial_port.BluetoothAssignedNumber16().has_value());
		EXPECT_EQ(*serial_port.BluetoothAssignedNumber16(), 0x1101U);
		ASSERT_TRUE(serial_port.BluetoothAssignedNumber32().has_value());
		EXPECT_EQ(*serial_port.BluetoothAssignedNumber32(), 0x00001101U);

		EXPECT_EQ(TUuid(U"1101"), serial_port);
		EXPECT_EQ(TUuid(U"00001101"), serial_port);
		EXPECT_EQ(TUuid(U"00001101-0000-1000-8000-00805f9b34fb"), serial_port);

		const TUuid custom(U"12345678-1234-5678-9abc-def012345678");
		EXPECT_EQ(static_cast<TString>(custom), U"12345678-1234-5678-9abc-def012345678");
		EXPECT_FALSE(custom.BluetoothAssignedNumber16().has_value());
		EXPECT_FALSE(custom.BluetoothAssignedNumber32().has_value());
		EXPECT_THROW(TUuid(U"not-a-uuid"), TInvalidArgumentException);
	}

	TEST(io_net_bluetooth_service, ProtocolAndFilterHelpers)
	{
		const TUuid custom_profile(U"12345678-1234-5678-9abc-def012345678");
		const TService service(
			0x12345678U,
			TString(U"GPS NMEA Tether"),
			TString(U"GPS data"),
			TString(U"Example"),
			TList<TUuid> { UUID_SERVICE_SERIAL_PORT },
			TList<TProfileDescriptor> { TProfileDescriptor(UUID_SERVICE_SERIAL_PORT, 0x0102U), TProfileDescriptor(custom_profile, 0x0200U) },
			TList<TProtocolDescriptor>
			{
				TProtocolDescriptor(UUID_PROTOCOL_L2CAP, TList<u64_t>()),
				TProtocolDescriptor(UUID_PROTOCOL_RFCOMM, TList<u64_t> { 21U }),
			}
		);

		EXPECT_TRUE(service.HasServiceClass(UUID_SERVICE_SERIAL_PORT));
		EXPECT_TRUE(service.HasProfile(custom_profile));
		EXPECT_TRUE(service.HasProtocol(UUID_PROTOCOL_RFCOMM));
		ASSERT_TRUE(service.RfcommChannel().has_value());
		EXPECT_EQ(*service.RfcommChannel(), 21U);
		EXPECT_FALSE(service.L2capPsm().has_value());

		TServiceDiscoveryFilter filter;
		filter.service_class_uuids.Append(UUID_SERVICE_SERIAL_PORT);
		filter.protocol_uuids.Append(UUID_PROTOCOL_RFCOMM);
		filter.name = U"GPS NMEA";
		filter.name_match = EServiceNameMatch::PREFIX;
		EXPECT_TRUE(filter.Matches(service));

		filter.name = U"Tether";
		filter.name_match = EServiceNameMatch::EXACT;
		EXPECT_FALSE(filter.Matches(service));

		filter.name_match = EServiceNameMatch::CONTAINS;
		EXPECT_TRUE(filter.Matches(service));
	}
}
