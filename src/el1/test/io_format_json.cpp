#include <gtest/gtest.h>
#include <el1/io_format_json.hpp>
#include <el1/io_file.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::io::format::json;
	using namespace el1::io::file;

	TEST(io_format_json, TJsonValue_Parse)
	{
		{
			const TJsonValue json = TJsonValue::Parse("12345");
			EXPECT_EQ(json.Type(), EType::NUMBER);
			EXPECT_EQ(json.Number(), 12345.0);
		}

		{
			const TJsonValue json = TJsonValue::Parse(".55");
			EXPECT_EQ(json.Type(), EType::NUMBER);
			EXPECT_EQ(json.Number(), 0.55);
		}

		{
			const TJsonValue json = TJsonValue::Parse("-.55");
			EXPECT_EQ(json.Type(), EType::NUMBER);
			EXPECT_EQ(json.Number(), -0.55);
		}

		{
			const TJsonValue json = TJsonValue::Parse("-0.55");
			EXPECT_EQ(json.Type(), EType::NUMBER);
			EXPECT_EQ(json.Number(), -0.55);
		}

		{
			const TJsonValue json = TJsonValue::Parse("12345.778557");
			EXPECT_EQ(json.Type(), EType::NUMBER);
			EXPECT_EQ(json.Number(), 12345.778557);
		}

		{
			const TJsonValue json = TJsonValue::Parse("\"hello world\"");
			EXPECT_EQ(json.Type(), EType::STRING);
			EXPECT_EQ(json.String(), "hello world");
		}

		{
			const TJsonValue json = TJsonValue::Parse("\"hello\\\"\\\\world\"");
			EXPECT_EQ(json.Type(), EType::STRING);
			EXPECT_EQ(json.String(), "hello\"\\world");
		}

		{
			const TJsonValue json = TJsonValue::Parse("true");
			EXPECT_EQ(json.Type(), EType::BOOLEAN);
			EXPECT_EQ(json.Boolean(), true);
		}

		{
			const TJsonValue json = TJsonValue::Parse("false");
			EXPECT_EQ(json.Type(), EType::BOOLEAN);
			EXPECT_EQ(json.Boolean(), false);
		}

		{
			const TJsonValue json = TJsonValue::Parse("null");
			EXPECT_EQ(json.Type(), EType::NULLVALUE);
			EXPECT_EQ(json.IsNull(), true);
		}

		{
			const TJsonValue json = TJsonValue::Parse("[]");
			EXPECT_EQ(json.Type(), EType::ARRAY);
			EXPECT_EQ(json.Array().Count(), 0U);
		}

		{
			const TJsonValue json = TJsonValue::Parse("[ [],[] ]");
			EXPECT_EQ(json.Type(), EType::ARRAY);
			EXPECT_EQ(json.Array().Count(), 2U);
		}

		{
			const TJsonValue json = TJsonValue::Parse("[ [],[1] ]");
			EXPECT_EQ(json.Type(), EType::ARRAY);
			EXPECT_EQ(json.Array().Count(), 2U);
		}

		{
			const TJsonValue json = TJsonValue::Parse("[ \"test\", \"hello world\", 17.5,12.5,7,3,-2, [], [1,2,3] ]");
			EXPECT_EQ(json.Type(), EType::ARRAY);
			EXPECT_EQ(json.Array().Count(), 9U);
			EXPECT_EQ(json[0].String(), "test");
			EXPECT_EQ(json[1].String(), "hello world");
			EXPECT_EQ(json[2].Number(), 17.5);
			EXPECT_EQ(json[3].Number(), 12.5);
			EXPECT_EQ(json[4].Number(), 7);
			EXPECT_EQ(json[5].Number(), 3);
			EXPECT_EQ(json[6].Number(), -2);
			EXPECT_EQ(json[7].Array().Count(), 0U);
			EXPECT_EQ(json[8].Array().Count(), 3U);
			EXPECT_EQ(json[8][0].Number(), 1);
			EXPECT_EQ(json[8][1].Number(), 2);
			EXPECT_EQ(json[8][2].Number(), 3);
		}

		{
			const TJsonValue json = TJsonValue::Parse("{}");
			EXPECT_EQ(json.Type(), EType::MAP);
			EXPECT_EQ(json.Map().Items().Count(), 0U);
		}

		{
			const TJsonValue json = TJsonValue::Parse("{ \"key1\":{}, \"key2\": []}");
			EXPECT_EQ(json.Type(), EType::MAP);
			EXPECT_EQ(json.Map().Items().Count(), 2U);
			EXPECT_EQ(json["key1"].Type(), EType::MAP);
			EXPECT_EQ(json["key2"].Type(), EType::ARRAY);
		}

		{
			EXPECT_THROW(TJsonValue::Parse("\"\n\""), TInvalidJsonException);
			EXPECT_THROW(TJsonValue::Parse("\"\\I\""), TInvalidJsonException);
			EXPECT_THROW(TJsonValue::Parse("-"), TInvalidJsonException);
			EXPECT_THROW(TJsonValue::Parse("a"), TInvalidJsonException);
			EXPECT_THROW(TJsonValue::Parse(":"), TInvalidJsonException);
			EXPECT_THROW(TJsonValue::Parse("\"test"), TInvalidJsonException);
			EXPECT_THROW(TJsonValue::Parse("[1,2,3,]"), TInvalidJsonException);
			EXPECT_THROW(TJsonValue::Parse("{\"key1}\":1,}"), TInvalidJsonException);
			EXPECT_THROW(TJsonValue::Parse("17").String() += "abc", TException);
		}

		{
			const TJsonValue json = TJsonValue::Parse(TFile("../../../testdata/test1.json"));
		}

		{
			const TJsonValue json = TJsonValue::Parse("\" \\b\\f\\n\\r\\t\\\" \"");
			EXPECT_EQ(json.Type(), EType::STRING);
			EXPECT_EQ(json.String(), " \b\f\n\r\t\" ");
		}
	}

	TEST(io_format_json, JsonUnquote)
	{
		EXPECT_EQ(JsonUnquote("\" abc 123 \\b\\f\\n\\r\\t\\\"\\\\ \""), " abc 123 \b\f\n\r\t\"\\ ");
	}

	TEST(io_format_json, TJsonValue_construct)
	{
		{
			TJsonValue value = true;
			EXPECT_EQ(value.Type(), EType::BOOLEAN);
			EXPECT_EQ(value.Boolean(), true);
		}

		{
			TJsonValue value = 12.0;
			EXPECT_EQ(value.Type(), EType::NUMBER);
			EXPECT_EQ(value.Number(), 12.0);
		}

		{
			TString str = "hello world";
			TJsonValue value = str;
			EXPECT_EQ(value.Type(), EType::STRING);
			EXPECT_EQ(value.String(), "hello world");
		}

		{
			TJsonValue value = TString("hello world");
			EXPECT_EQ(value.Type(), EType::STRING);
			EXPECT_EQ(value.String(), "hello world");
		}

		{
			TJsonValue value = "hello world";
			EXPECT_EQ(value.Type(), EType::STRING);
			EXPECT_EQ(value.String(), "hello world");
		}

		{
			TList<TJsonValue> list = {1.0, 2.0, 3.0};
			TJsonValue value = list;
			EXPECT_EQ(value.Type(), EType::ARRAY);
			EXPECT_EQ(value.Array().Count(), 3U);
		}

		{
			TJsonValue value = TList<TJsonValue>({1.0,2.0,3.0});
			EXPECT_EQ(value.Type(), EType::ARRAY);
			EXPECT_EQ(value.Array().Count(), 3U);
		}

		{
			TSortedMap<TString, TJsonValue> map;
			map.Add("test", true);
			TJsonValue value = std::move(map);
			EXPECT_EQ(value.Type(), EType::MAP);
			EXPECT_EQ(value.Map().Items().Count(), 1U);
		}
	}

	TEST(io_format_json, TJsonValue_assign)
	{
		{
			TJsonValue v1;
			TJsonValue v2 = false;

			EXPECT_FALSE(v2.IsNull());
			v2 = v1;
			EXPECT_TRUE(v2.IsNull());
		}

		{
			TJsonValue v1 = true;
			TJsonValue v2;
			v2 = v1;

			EXPECT_EQ(v2.Type(), EType::BOOLEAN);
			EXPECT_EQ(v2.Boolean(), true);
		}

		{
			TJsonValue v1 = 17.5;
			TJsonValue v2;
			v2 = v1;

			EXPECT_EQ(v2.Type(), EType::NUMBER);
			EXPECT_EQ(v2.Number(), 17.5);
		}

		{
			TJsonValue v1 = "hello world";
			TJsonValue v2;
			v2 = v1;

			EXPECT_EQ(v2.Type(), EType::STRING);
			EXPECT_EQ(v2.String(), "hello world");
		}

		{
			TJsonValue v2;
			v2 = "test";

			EXPECT_EQ(v2.Type(), EType::STRING);
			EXPECT_EQ(v2.String(), "test");
		}

		{
			TJsonValue v1 = TList<TJsonValue>({1.0, 2.0, 3.0});
			TJsonValue v2;
			v2 = v1;

			EXPECT_EQ(v2.Type(), EType::ARRAY);
			EXPECT_EQ(v2.Array().Count(), 3U);
		}

		{
			TJsonValue v2;
			EXPECT_TRUE(v2.IsNull());
			v2 = TList<TJsonValue>({1.0, 2.0, 3.0});
			EXPECT_EQ(v2.Type(), EType::ARRAY);
			EXPECT_EQ(v2.Array().Count(), 3U);
		}

		{
			TSortedMap<TString, TJsonValue> map;
			map.Add("test", true);

			TJsonValue v1 = map;
			TJsonValue v2;
			v2 = v1;

			EXPECT_EQ(v2.Type(), EType::MAP);
			EXPECT_EQ(v2.Map().Items().Count(), 1U);
		}
	}

	TEST(io_format_json, TJsonValue_wrong_datatype)
	{
		{
			TJsonValue value;
			EXPECT_THROW(value.Boolean() = true, TException);
			EXPECT_THROW(value.Number() = 17.3, TException);
			EXPECT_THROW(value.String() = "test", TException);
			EXPECT_THROW(value.Array().Clear(), TException);
			EXPECT_THROW(value.Map().Remove("test"), TException);
		}

		{
			TJsonValue value = true;
			EXPECT_THROW(value.Number() = 17.3, TException);
			EXPECT_THROW(value.String() = "test", TException);
			EXPECT_THROW(value.Array().Clear(), TException);
			EXPECT_THROW(value.Map().Remove("test"), TException);
		}

		{
			TJsonValue value = 12.3;
			EXPECT_THROW(value.Boolean() = true, TException);
			EXPECT_THROW(value.String() = "test", TException);
			EXPECT_THROW(value.Array().Clear(), TException);
			EXPECT_THROW(value.Map().Remove("test"), TException);
		}

		{
			TJsonValue value = "test";
			EXPECT_THROW(value.Boolean() = true, TException);
			EXPECT_THROW(value.Number() = 17.3, TException);
			EXPECT_THROW(value.Array().Clear(), TException);
			EXPECT_THROW(value.Map().Remove("test"), TException);
		}

		{
			TJsonValue value = TList<TJsonValue>();
			EXPECT_THROW(value.Boolean() = true, TException);
			EXPECT_THROW(value.Number() = 17.3, TException);
			EXPECT_THROW(value.String() = "test", TException);
			EXPECT_THROW(value.Map().Remove("test"), TException);
		}

		{
			TJsonValue value = TSortedMap<TString, TJsonValue>();
			EXPECT_THROW(value.Boolean() = true, TException);
			EXPECT_THROW(value.Number() = 17.3, TException);
			EXPECT_THROW(value.String() = "test", TException);
			EXPECT_THROW(value.Array().Clear(), TException);
		}
	}

	TEST(io_format_json, TJsonValue_Pipe)
	{
		const TString str = TJsonValue::Parse("[ \"test\", \"hello world\", 17,12,{\"foo\":\"bar\",  \"abc\":123}   ,7,3,-2, [], [1,2,3] ]").Pipe().Collect();
		EXPECT_EQ(str, TString("[\"test\",\"hello world\",17,12,{\"abc\":123,\"foo\":\"bar\"},7,3,-2,[],[1,2,3]]"));
	}

	TEST(io_format_json, TJsonValue_ToString)
	{
		TJsonValue json = TJsonValue::Parse("[ null,true, false, \"test\\\\\\n\\t\", \"hello world\", 17,12,{\"foo\":\"bar\",  \"abc\":123}   ,7,3,-2, [], [1,2,3] ]");

		json.Array().Insert(0, "\x05");
		TString str = json.ToString();
		EXPECT_EQ(str, TString("[\"\\u0005\",null,true,false,\"test\\\\\\n\\t\",\"hello world\",17,12,{\"abc\":123,\"foo\":\"bar\"},7,3,-2,[],[1,2,3]]"));
	}
}
