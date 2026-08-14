#include <gtest/gtest.h>
#include <el1/debug.hpp>
#include <string_view>

using namespace ::testing;

namespace el1::test::debug
{
	struct TTypeNameTest {};
}

namespace
{
	TEST(debug, GetTypeName)
	{
		constexpr el1::debug::TTypeNameView type_name = el1::debug::GetTypeName<el1::test::debug::TTypeNameTest>();
		static_assert(std::string_view(type_name.data, type_name.length) == "el1::test::debug::TTypeNameTest");
		EXPECT_EQ(std::string_view(type_name.data, type_name.length), "el1::test::debug::TTypeNameTest");
	}
}
