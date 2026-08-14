#include <gtest/gtest.h>
#include <el1/security_gssapi.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::security::gssapi;

	TEST(security_gssapi, imports_host_based_service_name)
	{
		EXPECT_NO_THROW(TInitiatorContext(el1::io::text::string::TString(U"HTTP@example.com")));
	}
}
