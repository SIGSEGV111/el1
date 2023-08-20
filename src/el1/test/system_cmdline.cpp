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
			TStringArgument(&name, 'n', L"name", L"", false, false, L"sets the name"),
			TIntegerArgument(&age, 'a', L"age", L"", false, false, L"sets the age"),
			TFloatArgument(&weight, 'w', L"weight", L"", false, false, L"sets the weight"),
			TFlagArgument(&has_appointment, 'p', L"has-appointment", L"", L"some flag"),
			TFlagArgument(&some_flag, 'f', L"some-flag", L"", L"some flag"),
			TStringArgument(&anonymous, '\0', L"", L"", true, true, L"some anonymous value"),
			TFlagArgument(&some_other_flag, 'o', L"some-other-flag", L"", L"some other flag"),
			TBooleanArgument(&bool_arg, 'b', L"abc", L"", true, false, L"some optional bool"),
			TArrayArgument(&array, ",", 'r', L"array", L"", false, false, L"an array"),
			TPathArgument(&path, EObjectType::DIRECTORY, ECreateMode::OPEN, 'z', L"path", L"", false, false, L"some path")
		);

		EXPECT_EQ(n_processed, 13U);
		EXPECT_EQ(name, L"foobar");
		EXPECT_EQ(age, 71);
		EXPECT_EQ(weight, 82.34);
		EXPECT_FALSE(has_appointment);
		EXPECT_TRUE(some_flag);
		EXPECT_TRUE(bool_arg);
		EXPECT_EQ(array.Count(), 5U);
		EXPECT_EQ((TString)path, "/etc");
		EXPECT_FALSE(some_other_flag);
		EXPECT_EQ(anonymous, L"anonymous arg value");
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
			TStringArgument(&name, 'n', L"name", L"", false, false, L"sets the name"),
			TIntegerArgument(&age, 'a', L"age", L"", false, false, L"sets the age"),
			TFloatArgument(&weight, 'w', L"weight", L"", false, false, L"sets the weight"),
			TFlagArgument(&has_appointment, 'p', L"has-appointment", L"", L"some flag"),
			TFlagArgument(&some_flag, 'f', L"some-flag", L"", L"some flag"),
			TStringArgument(&anonymous, '\0', L"", L"", true, true, L"some anonymous value"),
			TFlagArgument(&some_other_flag, 'o', L"some-other-flag", L"", L"some other flag"),
			TBooleanArgument(&bool_arg, 'b', L"abc", L"", true, false, L"some optional bool"),
			TArrayArgument(&array, ",", 'r', L"array", L"", false, false, L"an array"),
			TPathArgument(&path, EObjectType::DIRECTORY, ECreateMode::OPEN, 'z', L"path", L"", false, false, L"some path")
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
			TStringArgument(&name, 'n', L"name", L"", false, false, L"sets the name"),
			TIntegerArgument(&age, 'a', L"age", L"", false, false, L"sets the age"),
			TFloatArgument(&weight, 'w', L"weight", L"", false, false, L"sets the weight"),
			TFlagArgument(&has_appointment, 'p', L"has-appointment", L"", L"some flag"),
			TFlagArgument(&some_flag, 'f', L"some-flag", L"", L"some flag"),
			TStringArgument(&anonymous, '\0', L"", L"", true, true, L"some anonymous value"),
			TFlagArgument(&some_other_flag, 'o', L"some-other-flag", L"", L"some other flag"),
			TBooleanArgument(&bool_arg, 'b', L"abc", L"", true, false, L"some optional bool"),
			TArrayArgument(&array, ",", 'r', L"array", L"", false, false, L"an array"),
			TPathArgument(&path, EObjectType::DIRECTORY, ECreateMode::OPEN, 'z', L"path", L"", false, false, L"some path")
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
			TStringArgument(&name, 'n', L"name", L"", false, false, L"sets the name"),
			TIntegerArgument(&age, 'a', L"age", L"", false, false, L"sets the age"),
			TFloatArgument(&weight, 'w', L"weight", L"", false, false, L"sets the weight"),
			TFlagArgument(&has_appointment, 'p', L"has-appointment", L"", L"some flag"),
			TFlagArgument(&some_flag, 'f', L"some-flag", L"", L"some flag"),
			TStringArgument(&anonymous, '\0', L"", L"", true, true, L"some anonymous value"),
			TFlagArgument(&some_other_flag, 'o', L"some-other-flag", L"", L"some other flag"),
			TBooleanArgument(&bool_arg, 'b', L"abc", L"", true, false, L"some optional bool"),
			TArrayArgument(&array, ",", 'r', L"array", L"", false, false, L"an array"),
			TPathArgument(&path, EObjectType::DIRECTORY, ECreateMode::OPEN, 'z', L"path", L"", false, false, L"some path")
		), TException);
	}
}
