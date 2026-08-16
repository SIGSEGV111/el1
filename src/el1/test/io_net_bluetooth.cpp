#include <gtest/gtest.h>
#include <el1/error.hpp>
#include <el1/io_net_bluetooth.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::error;
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
}
