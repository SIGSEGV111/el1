#include <gtest/gtest.h>
#include <el1/io_serialization_json.hpp>
#include <el1/io_serialization_binary.hpp>

using namespace ::testing;
using namespace el1;
using namespace el1::io::types;
using namespace el1::io::text::string;
using namespace el1::io::collection::list;
using namespace el1::io::collection::map;
using namespace el1::io::format::json;

namespace serialization_test
{
	enum class EMode : s16_t { OFF = -1, ON = 7 };

	struct TChild
	{
		s32_t id = 0;
		TString name;
	};

	struct TRoot
	{
		s64_t big = 0;
		u32_t count = 0;
		double ratio = 0;
		TString title;
		TList<TChild> children;
		TSortedMap<TString, s32_t> scores;
		std::optional<TString> note;
		EMode mode = EMode::OFF;
		s32_t added_in_v2 = 1234;
	};

	struct TCompact
	{
		s32_t value = 0;
	};
}

EL_SERIALIZABLE(serialization_test::TChild, 1,
	EL_SERIALIZATION_MEMBER(id),
	EL_SERIALIZATION_MEMBER(name));

EL_SERIALIZABLE(serialization_test::TRoot, 2,
	EL_SERIALIZATION_MEMBER(big),
	EL_SERIALIZATION_MEMBER(count),
	EL_SERIALIZATION_MEMBER(ratio),
	EL_SERIALIZATION_MEMBER(title),
	EL_SERIALIZATION_MEMBER(children),
	EL_SERIALIZATION_MEMBER(scores),
	EL_SERIALIZATION_MEMBER(note),
	EL_SERIALIZATION_MEMBER(mode),
	EL_SERIALIZATION_MEMBER(added_in_v2, 2));

namespace el1::io::serialization
{
	template<>
	struct TCodec<serialization_test::TCompact>
	{
		template<typename TArchive>
		static void Serialize(TArchive& archive, const serialization_test::TCompact& value)
		{
			archive.Signed(value.value);
		}

		template<typename TArchive>
		static void Deserialize(TArchive& archive, serialization_test::TCompact& value)
		{
			value.value = (s32_t)archive.Signed();
		}
	};
}

namespace
{
	using namespace el1::io::serialization;

	serialization_test::TRoot Sample()
	{
		serialization_test::TRoot value;
		value.big = std::numeric_limits<s64_t>::max();
		value.count = 42;
		value.ratio = 0.125;
		value.title = TString(U"hello 😀");
		value.children.Append({7, TString(U"first")});
		value.children.Append({8, TString(U"second")});
		value.scores.Add(TString(U"alpha"), 11);
		value.scores.Add(TString(U"beta"), -7);
		value.note = TString(U"optional");
		value.mode = serialization_test::EMode::ON;
		value.added_in_v2 = 99;
		return value;
	}

	TEST(io_serialization, SchemaMemberMacroUsesEnclosingTypeAndForwardsMetadata)
	{
		constexpr auto members = TSchema<serialization_test::TRoot>::Members();
		EXPECT_EQ(std::get<0>(members).info.name, TStringView(U"big"));
		EXPECT_EQ(std::get<0>(members).pointer, &serialization_test::TRoot::big);
		EXPECT_EQ(std::get<8>(members).info.name, TStringView(U"added_in_v2"));
		EXPECT_EQ(std::get<8>(members).info.since_version, 2U);
		EXPECT_EQ(std::get<8>(members).pointer, &serialization_test::TRoot::added_in_v2);
	}

	TEST(io_serialization, JsonSchemaMetadataAndRoundTrip)
	{
		const auto source = Sample();
		TJsonValue json = json::ToValue(source);
		ASSERT_TRUE(json.IsMap());

		const TString metadata_key(U"$el1");
		const auto& metadata = json.Map()[metadata_key].Map();
		EXPECT_EQ(metadata[TString(U"format")].ToInteger<u32_t>(), 1U);
		EXPECT_EQ(metadata[TString(U"version")].ToInteger<u32_t>(), 2U);
		EXPECT_EQ(metadata[TString(U"type")].String(), TString(U"serialization_test::TRoot"));
		EXPECT_EQ(metadata[TString(U"type_id")].String().Length(), 32U);
		EXPECT_TRUE(json.Map()[TString(U"big")].IsNumber());
		EXPECT_EQ(json.Map()[TString(U"big")].ToInteger<s64_t>(), std::numeric_limits<s64_t>::max());

		const auto& child = json.Map()[TString(U"children")].Array()[0];
		ASSERT_TRUE(child.IsMap());
		EXPECT_EQ(child.Map()[metadata_key].Map()[TString(U"version")].ToInteger<u32_t>(), 1U);

		const TString encoded = json.ToString();
		EXPECT_NE(encoded.Find(TStringView(U"9223372036854775807")), NEG1);
		const auto decoded = json::FromString<serialization_test::TRoot>(encoded);
		EXPECT_EQ(decoded.big, source.big);
		EXPECT_EQ(decoded.count, source.count);
		EXPECT_DOUBLE_EQ(decoded.ratio, source.ratio);
		EXPECT_EQ(decoded.title, source.title);
		ASSERT_EQ(decoded.children.Count(), 2U);
		EXPECT_EQ(decoded.children[0].id, 7);
		EXPECT_EQ(decoded.children[1].name, TString(U"second"));
		EXPECT_EQ(decoded.scores[TString(U"alpha")], 11);
		ASSERT_TRUE(decoded.note.has_value());
		EXPECT_EQ(*decoded.note, TString(U"optional"));
		EXPECT_EQ(decoded.mode, serialization_test::EMode::ON);
		EXPECT_EQ(decoded.added_in_v2, 99);
	}

	TEST(io_serialization, OlderSchemaLeavesNewFieldAtDefault)
	{
		TJsonValue json = json::ToValue(Sample());
		json.Map()[TString(U"$el1")].Map()[TString(U"version")] = (s64_t)1;
		json.Map().Remove(TString(U"added_in_v2"));

		serialization_test::TRoot target;
		target.added_in_v2 = 777;
		json::FromValue(json, target);
		EXPECT_EQ(target.big, std::numeric_limits<s64_t>::max());
		EXPECT_EQ(target.added_in_v2, 777);
	}

	TEST(io_serialization, TypeMismatchRejected)
	{
		const TJsonValue json = json::ToValue(Sample());
		EXPECT_THROW((json::FromValue<serialization_test::TChild>(json)), error::TException);
	}

	TEST(io_serialization, CustomCodecOverridesSchema)
	{
		const serialization_test::TCompact source{123};
		const TJsonValue json = json::ToValue(source);
		ASSERT_TRUE(json.IsNumber());
		EXPECT_EQ(json.ToInteger<s64_t>(), 123);
		EXPECT_EQ(json::FromValue<serialization_test::TCompact>(json).value, 123);
	}

	TEST(io_serialization, PackedBinaryRoundTrip)
	{
		const auto source = Sample();
		auto bytes = binary::packed::ToBytes(source);
		ASSERT_GT(bytes.Count(), 5U);
		EXPECT_EQ(bytes[0], (byte_t)'E');
		EXPECT_EQ(bytes[1], (byte_t)'L');
		EXPECT_EQ(bytes[2], (byte_t)'1');
		EXPECT_EQ(bytes[3], (byte_t)'S');

		const auto decoded = binary::packed::FromBytes<serialization_test::TRoot>(std::move(bytes));
		EXPECT_EQ(decoded.big, source.big);
		EXPECT_EQ(decoded.count, source.count);
		EXPECT_DOUBLE_EQ(decoded.ratio, source.ratio);
		EXPECT_EQ(decoded.title, source.title);
		ASSERT_EQ(decoded.children.Count(), source.children.Count());
		EXPECT_EQ(decoded.children[1].name, source.children[1].name);
		EXPECT_EQ(decoded.scores[TString(U"beta")], -7);
		ASSERT_TRUE(decoded.note.has_value());
		EXPECT_EQ(*decoded.note, *source.note);
		EXPECT_EQ(decoded.mode, source.mode);
		EXPECT_EQ(decoded.added_in_v2, source.added_in_v2);
	}

	TEST(io_serialization, ViewsPointersAndArraysRequireCodec)
	{
		static_assert(REQUIRES_EXPLICIT_CODEC<int*>);
		static_assert(REQUIRES_EXPLICIT_CODEC<int[4]>);
		static_assert(REQUIRES_EXPLICIT_CODEC<TStringView>);
		static_assert(REQUIRES_EXPLICIT_CODEC<io::collection::array::array_t<const byte_t>>);
	}

	TEST(io_serialization, OptionalNullRoundTrip)
	{
		auto source = Sample();
		source.note.reset();
		const TJsonValue json = json::ToValue(source);
		EXPECT_TRUE(json.Map()[TString(U"note")].IsNull());
		EXPECT_FALSE(json::FromValue<serialization_test::TRoot>(json).note.has_value());

		auto bytes = binary::packed::ToBytes(source);
		EXPECT_FALSE(binary::packed::FromBytes<serialization_test::TRoot>(std::move(bytes)).note.has_value());
	}

	TEST(io_serialization, JsonRejectsNonFiniteNumbers)
	{
		EXPECT_THROW(json::ToValue(std::numeric_limits<double>::infinity()), error::TException);
		EXPECT_THROW(json::ToValue(std::numeric_limits<double>::quiet_NaN()), error::TException);
	}

	TEST(io_serialization, TypeIdRoundTrip)
	{
		constexpr auto info = TSchema<serialization_test::TRoot>::Info();
		const TString text = info.id.ToString();
		EXPECT_EQ(TTypeId::FromString(text.View()), info.id);
	}
}
