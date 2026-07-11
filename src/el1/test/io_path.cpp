#include <gtest/gtest.h>
#include <el1/io_path.hpp>

using namespace ::testing;

namespace
{
	using el1::io::path::TInvalidPathException;
	using el1::io::path::TPath;
	using el1::error::TInvalidArgumentException;

	TEST(io_path, delimiters)
	{
		TPath dot_path("root.branch.leaf", '.');
		ASSERT_EQ(dot_path.Components().Count(), 3U);
		EXPECT_TRUE(dot_path.Components()[0] == "root");
		EXPECT_TRUE(dot_path.Components()[1] == "branch");
		EXPECT_TRUE(dot_path.Components()[2] == "leaf");
		EXPECT_TRUE(dot_path.ToString() == "root.branch.leaf");
		EXPECT_TRUE(dot_path.Parent().ToString() == "root.branch");

		dot_path += TPath("item", '.');
		EXPECT_TRUE(dot_path.ToString() == "root.branch.leaf.item");
		dot_path -= TPath("leaf.item", '.');
		EXPECT_TRUE(dot_path.ToString() == "root.branch");

		const TPath unix_path("root/branch/leaf", '/');
		EXPECT_TRUE(unix_path.ToString() == "root/branch/leaf");

		const TPath windows_path("root\\branch\\leaf", '\\');
		EXPECT_TRUE(windows_path.ToString() == "root\\branch\\leaf");

		EXPECT_THROW(TPath("root..leaf", '.'), TInvalidPathException);
		EXPECT_THROW(dot_path += TPath("other/item", '/'), TInvalidArgumentException);
	}

	TEST(io_path, empty_and_rebase)
	{
		TPath path("root.branch.leaf", '.');
		path.Rebase(TPath("root.branch", '.'), TPath("other", '.'));
		EXPECT_TRUE(path.ToString() == "other.leaf");

		TPath single("root", '.');
		EXPECT_TRUE(single.Parent().IsEmpty());
		EXPECT_TRUE(single.Parent().ToString() == "");
	}
}
