#include <gtest/gtest.h>
#include <el1/system_cmdline.hpp>
#include <el1/error.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::system::cmdline;
	using namespace el1::io::file;
	using namespace el1::io::text;
	using namespace el1::error;
	using namespace el1::io::text::string;
	using namespace el1::io::collection::list;

	TEST(system_cmdline, ParseCmdlineArguments_basics)
	{
		TList<const char*> args = { "/path/to/exe", "--name=foobar", "--age=71", "--weight", "82.34", "-f", "--has-appointment=no", "--path=/etc", "-r", "apple,blub,candy,dude,emil", "-b", "true", "--", "anonymous arg value", nullptr };

		TString name, anonymous;
		s64_t age;
		double weight;
		bool has_appointment, some_flag, some_other_flag, bool_arg = false;
		TList<TString> array;
		TPath path;

		const usys_t n_processed = ParseCmdlineArguments(
			args.Count() - 1,
			args.ItemPtr(0),
			TStringArgument(&name, 'n', U"name", U"", false, false, U"sets the name"),
			TIntegerArgument(&age, 'a', U"age", U"", false, false, U"sets the age"),
			TFloatArgument(&weight, 'w', U"weight", U"", false, false, U"sets the weight"),
			TFlagArgument(&has_appointment, 'p', U"has-appointment", U"", U"some flag"),
			TFlagArgument(&some_flag, 'f', U"some-flag", U"", U"some flag"),
			TStringArgument(&anonymous, '\0', U"", U"", true, true, U"some anonymous value"),
			TFlagArgument(&some_other_flag, 'o', U"some-other-flag", U"", U"some other flag"),
			TBooleanArgument(&bool_arg, 'b', U"abc", U"", true, false, U"some optional bool"),
			TArrayArgument(&array, U",", 'r', U"array", U"", false, false, U"an array"),
			TPathArgument(&path, EObjectType::DIRECTORY, ECreateMode::OPEN, 'z', U"path", U"", false, false, U"some path")
		);

		EXPECT_EQ(n_processed, 13U);
		EXPECT_EQ(name, U"foobar");
		EXPECT_EQ(age, 71);
		EXPECT_EQ(weight, 82.34);
		EXPECT_FALSE(has_appointment);
		EXPECT_TRUE(some_flag);
		EXPECT_TRUE(bool_arg);
		EXPECT_EQ(array.Count(), 5U);
		EXPECT_EQ((TString)path, U"/etc");
		EXPECT_FALSE(some_other_flag);
		EXPECT_EQ(anonymous, U"anonymous arg value");
	}

	TEST(system_cmdline, ParseCmdlineArguments_help)
	{
		TList<const char*> args = { "/path/to/exe", "--help", "--name=foobar", "--age=71", "--weight", "82.34", "-f", "--has-appointment=no", "--path=/etc", "-r", "apple,blub,candy,dude,emil", "-b", "true", "--", "anonymous arg value", nullptr };

		TString name, anonymous;
		s64_t age;
		double weight;
		bool has_appointment, some_flag, some_other_flag, bool_arg = false;
		TList<TString> array;
		TPath path;

		EXPECT_THROW(ParseCmdlineArguments(
			args.Count() - 1,
			args.ItemPtr(0),
			TStringArgument(&name, 'n', U"name", U"", false, false, U"sets the name"),
			TIntegerArgument(&age, 'a', U"age", U"", false, false, U"sets the age"),
			TFloatArgument(&weight, 'w', U"weight", U"", false, false, U"sets the weight"),
			TFlagArgument(&has_appointment, 'p', U"has-appointment", U"", U"some flag"),
			TFlagArgument(&some_flag, 'f', U"some-flag", U"", U"some flag"),
			TStringArgument(&anonymous, '\0', U"", U"", true, true, U"some anonymous value"),
			TFlagArgument(&some_other_flag, 'o', U"some-other-flag", U"", U"some other flag"),
			TBooleanArgument(&bool_arg, 'b', U"abc", U"", true, false, U"some optional bool"),
			TArrayArgument(&array, U",", 'r', U"array", U"", false, false, U"an array"),
			TPathArgument(&path, EObjectType::DIRECTORY, ECreateMode::OPEN, 'z', U"path", U"", false, false, U"some path")
		), shutdown_t);
	}

	TEST(system_cmdline, ParseCmdlineArguments_unrecognoized_arg)
	{
		TList<const char*> args = { "/path/to/exe", "--name=foobar", "--age=71", "--weight", "82.34", "-f", "--has-appointment=no", "--path=/etc", "-r", "apple,blub,candy,dude,emil", "--nonsense", "-b", "true", "--", "anonymous arg value", nullptr };

		TString name, anonymous;
		s64_t age;
		double weight;
		bool has_appointment, some_flag, some_other_flag, bool_arg = false;
		TList<TString> array;
		TPath path;

		EXPECT_THROW(ParseCmdlineArguments(
			args.Count() - 1,
			args.ItemPtr(0),
			TStringArgument(&name, 'n', U"name", U"", false, false, U"sets the name"),
			TIntegerArgument(&age, 'a', U"age", U"", false, false, U"sets the age"),
			TFloatArgument(&weight, 'w', U"weight", U"", false, false, U"sets the weight"),
			TFlagArgument(&has_appointment, 'p', U"has-appointment", U"", U"some flag"),
			TFlagArgument(&some_flag, 'f', U"some-flag", U"", U"some flag"),
			TStringArgument(&anonymous, '\0', U"", U"", true, true, U"some anonymous value"),
			TFlagArgument(&some_other_flag, 'o', U"some-other-flag", U"", U"some other flag"),
			TBooleanArgument(&bool_arg, 'b', U"abc", U"", true, false, U"some optional bool"),
			TArrayArgument(&array, U",", 'r', U"array", U"", false, false, U"an array"),
			TPathArgument(&path, EObjectType::DIRECTORY, ECreateMode::OPEN, 'z', U"path", U"", false, false, U"some path")
		), TException);
	}

	TEST(system_cmdline, ParseCmdlineArguments_missing_arg)
	{
		TList<const char*> args = { "/path/to/exe", "--age=71", "--weight", "82.34", "-f", "--has-appointment=no", "--path=/etc", "-r", "apple,blub,candy,dude,emil", "-b", "true", "--", "anonymous arg value", nullptr };

		TString name, anonymous;
		s64_t age;
		double weight;
		bool has_appointment, some_flag, some_other_flag, bool_arg = false;
		TList<TString> array;
		TPath path;

		EXPECT_THROW(ParseCmdlineArguments(
			args.Count() - 1,
			args.ItemPtr(0),
			TStringArgument(&name, 'n', U"name", U"", false, false, U"sets the name"),
			TIntegerArgument(&age, 'a', U"age", U"", false, false, U"sets the age"),
			TFloatArgument(&weight, 'w', U"weight", U"", false, false, U"sets the weight"),
			TFlagArgument(&has_appointment, 'p', U"has-appointment", U"", U"some flag"),
			TFlagArgument(&some_flag, 'f', U"some-flag", U"", U"some flag"),
			TStringArgument(&anonymous, '\0', U"", U"", true, true, U"some anonymous value"),
			TFlagArgument(&some_other_flag, 'o', U"some-other-flag", U"", U"some other flag"),
			TBooleanArgument(&bool_arg, 'b', U"abc", U"", true, false, U"some optional bool"),
			TArrayArgument(&array, U",", 'r', U"array", U"", false, false, U"an array"),
			TPathArgument(&path, EObjectType::DIRECTORY, ECreateMode::OPEN, 'z', U"path", U"", false, false, U"some path")
		), TException);
	}

	TEST(system_cmdline, ParseCmdlineArguments_unknown_path_arg)
	{
		TList<const char*> args = { "/path/to/exe", "--path=/etc/passwd", nullptr };
		TPath path;

		ParseCmdlineArguments(
			args.Count() - 1,
			args.ItemPtr(0),
			TPathArgument(&path, EObjectType::UNKNOWN, ECreateMode::OPEN, 'z', U"path", U"", false, false, U"some path")
		);

		EXPECT_EQ(path.ToString(), U"/etc/passwd");
	}
}
