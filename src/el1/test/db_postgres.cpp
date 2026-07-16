#ifdef EL1_WITH_POSTGRES
#include <gtest/gtest.h>
#include <el1/db_postgres.hpp>
#include <el1/io_format_json.hpp>
#include <el1/io_path.hpp>
#include <arpa/inet.h>
#include <cstring>
#include <stdexcept>

using namespace ::testing;

namespace
{
	using namespace el1::db::postgres;
	using namespace el1::io::text::string;
	using namespace el1::io::format::json;
	using namespace el1::io::collection::list;
	using namespace el1::io::collection::map;
	using namespace el1::error;

	const IDatatypeCodec& FindDatatypeCodec(const char* const datatype_name)
	{
		for(const IDatatypeCodec* const codec : TTypeMap::GLOBAL_CODEC_REGISTRY)
			if(strcmp(codec->datatype_name, datatype_name) == 0)
				return *codec;

		throw std::logic_error("datatype codec not found");
	}

	template<typename T>
	T RoundTripCodec(const IDatatypeCodec& codec, const T& input)
	{
		const usys_t capacity = codec.Serialize(&input, array_t<byte_t>());
		TList<byte_t> encoded;
		encoded.SetCount(capacity);
		const usys_t size = codec.Serialize(&input, encoded);
		const array_t<const byte_t> pg_value = size == 0
			? array_t<const byte_t>()
			: array_t<const byte_t>(encoded.ItemPtr(0), size);

		alignas(T) byte_t storage[sizeof(T)];
		codec.Deserialize(pg_value, storage);
		T* const decoded = reinterpret_cast<T*>(storage);
		T result = *decoded;
		codec.Destruct(decoded);
		return result;
	}

	TEST(db_postgres_codec, RegistryMetadata)
	{
		ASSERT_GE(TTypeMap::GLOBAL_CODEC_REGISTRY.Count(), 20U);
		const IDatatypeCodec& bool_codec = FindDatatypeCodec("bool");
		EXPECT_STREQ(bool_codec.namespace_name, "pg_catalog");
		EXPECT_EQ(*bool_codec.cxx_type_info, typeid(bool));
		EXPECT_EQ(bool_codec.cxx_size, sizeof(bool));
		EXPECT_EQ(bool_codec.cxx_alignment, alignof(bool));
		EXPECT_TRUE(bool_codec.required);

		const IDatatypeCodec& ltree_codec = FindDatatypeCodec("ltree");
		EXPECT_STREQ(ltree_codec.namespace_name, "*");
		EXPECT_FALSE(ltree_codec.required);
	}

	TEST(db_postgres_codec, BooleanAndCharacter)
	{
		const IDatatypeCodec& bool_codec = FindDatatypeCodec("bool");
		EXPECT_TRUE(RoundTripCodec(bool_codec, true));
		EXPECT_FALSE(RoundTripCodec(bool_codec, false));

		alignas(bool) byte_t bool_storage[sizeof(bool)];
		const byte_t invalid_bool[] = { 2 };
		EXPECT_THROW(bool_codec.Deserialize(array_t<const byte_t>(invalid_bool, sizeof(invalid_bool)), bool_storage), TException);
		EXPECT_THROW(bool_codec.Deserialize(array_t<const byte_t>(), bool_storage), TException);

		const IDatatypeCodec& char_codec = FindDatatypeCodec("char");
		EXPECT_EQ(RoundTripCodec(char_codec, 'Z'), 'Z');
		EXPECT_THROW(char_codec.Deserialize(array_t<const byte_t>(), bool_storage), TException);
	}

	TEST(db_postgres_codec, IntegersAndFloatingPoint)
	{
		const IDatatypeCodec& int2_codec = FindDatatypeCodec("int2");
		const IDatatypeCodec& int4_codec = FindDatatypeCodec("int4");
		const IDatatypeCodec& int8_codec = FindDatatypeCodec("int8");
		EXPECT_EQ(RoundTripCodec(int2_codec, (s16_t)-1234), (s16_t)-1234);
		EXPECT_EQ(RoundTripCodec(int4_codec, (s32_t)0x12345678), (s32_t)0x12345678);
		EXPECT_EQ(RoundTripCodec(int8_codec, (s64_t)-0x123456789ABCDELL), (s64_t)-0x123456789ABCDELL);

		const s32_t integer = 0x12345678;
		byte_t integer_buffer[4] = {};
		EXPECT_EQ(int4_codec.Serialize(&integer, array_t<byte_t>(integer_buffer, sizeof(integer_buffer))), 4U);
		EXPECT_EQ(integer_buffer[0], 0x12U);
		EXPECT_EQ(integer_buffer[1], 0x34U);
		EXPECT_EQ(integer_buffer[2], 0x56U);
		EXPECT_EQ(integer_buffer[3], 0x78U);
		EXPECT_THROW(int4_codec.Serialize(&integer, array_t<byte_t>(integer_buffer, 1)), TException);
		alignas(s32_t) byte_t integer_storage[sizeof(s32_t)];
		EXPECT_THROW(int4_codec.Deserialize(array_t<const byte_t>(integer_buffer, 3), integer_storage), TException);

		const IDatatypeCodec& float4_codec = FindDatatypeCodec("float4");
		const IDatatypeCodec& float8_codec = FindDatatypeCodec("float8");
		EXPECT_FLOAT_EQ(RoundTripCodec(float4_codec, 1.25F), 1.25F);
		EXPECT_DOUBLE_EQ(RoundTripCodec(float8_codec, -123.5), -123.5);
		alignas(double) byte_t float_storage[sizeof(double)];
		EXPECT_THROW(float8_codec.Deserialize(array_t<const byte_t>(integer_buffer, sizeof(integer_buffer)), float_storage), TException);
	}

	TEST(db_postgres_codec, ByteArrayAndStrings)
	{
		const IDatatypeCodec& bytea_codec = FindDatatypeCodec("bytea");
		const TList<byte_t> bytes = { 0, 1, 2, 127, 255 };
		const TList<byte_t> decoded_bytes = RoundTripCodec(bytea_codec, bytes);
		ASSERT_EQ(decoded_bytes.Count(), bytes.Count());
		for(usys_t i = 0; i < bytes.Count(); i++)
			EXPECT_EQ(decoded_bytes[i], bytes[i]);

		const IDatatypeCodec& text_codec = FindDatatypeCodec("text");
		const TString text("Grüße from PostgreSQL");
		EXPECT_EQ(RoundTripCodec(text_codec, text), text);

		const usys_t capacity = text_codec.Serialize(&text, array_t<byte_t>());
		ASSERT_GT(capacity, 1U);
		TList<byte_t> short_buffer;
		short_buffer.SetCount(capacity - 1U);
		EXPECT_THROW(text_codec.Serialize(&text, short_buffer), TException);
	}

	TEST(db_postgres_codec, TimestampJsonAndNoOp)
	{
		const IDatatypeCodec& timestamp_codec = FindDatatypeCodec("timestamp");
		const TTime timestamp = TTime::ConvertFrom(EUnit::MICROSECONDS, 946684800123456LL);
		EXPECT_EQ(RoundTripCodec(timestamp_codec, timestamp), timestamp);

		const IDatatypeCodec& json_codec = FindDatatypeCodec("json");
		const IDatatypeCodec& jsonb_codec = FindDatatypeCodec("jsonb");
		const TJsonValue json = TJsonValue::Parse("{\"answer\":42,\"ok\":true}");
		EXPECT_EQ(RoundTripCodec(json_codec, json).ToString(), json.ToString());
		EXPECT_EQ(RoundTripCodec(jsonb_codec, json).ToString(), json.ToString());

		alignas(TJsonValue) byte_t json_storage[sizeof(TJsonValue)];
		const byte_t invalid_jsonb[] = { 2, '{', '}' };
		EXPECT_THROW(jsonb_codec.Deserialize(array_t<const byte_t>(invalid_jsonb, sizeof(invalid_jsonb)), json_storage), TException);
		EXPECT_THROW(jsonb_codec.Deserialize(array_t<const byte_t>(), json_storage), TException);

		const IDatatypeCodec& void_codec = FindDatatypeCodec("void");
		EXPECT_EQ(void_codec.Serialize(nullptr, array_t<byte_t>()), 0U);
		void_codec.Deserialize(array_t<const byte_t>(), nullptr);
		void_codec.Destruct(nullptr);
	}

	TEST(db_postgres_codec, Ltree)
	{
		const IDatatypeCodec& codec = FindDatatypeCodec("ltree");
		const el1::io::path::TPath path("Top.Science.Astronomy", '.');
		const el1::io::path::TPath decoded = RoundTripCodec(codec, path);
		EXPECT_EQ(decoded.ToString(), path.ToString());
		EXPECT_EQ(decoded.Separator(), path.Separator());

		const el1::io::path::TPath invalid_path("Top/Science", '/');
		EXPECT_THROW(codec.Serialize(&invalid_path, array_t<byte_t>()), TInvalidArgumentException);

		alignas(el1::io::path::TPath) byte_t storage[sizeof(el1::io::path::TPath)];
		const byte_t invalid_version[] = { 0, 'T', 'o', 'p' };
		EXPECT_THROW(codec.Deserialize(array_t<const byte_t>(invalid_version, sizeof(invalid_version)), storage), TException);
	}

	TEST(db_postgres, TPostgresConnection_connect)
	{
		TSortedMap<TString,TString> properties;
		TPostgresConnection connection(properties);
	}


	TEST(db_postgres, simple_select)
	{
		TSortedMap<TString,TString> properties;
		TPostgresConnection connection(properties);

		{
			usys_t n_rows = 0;
			for(auto rs = connection.Execute("select 1,2,3.0::float8,'hello world',NULL,true,false,timestamp '2000-01-01',timestamp '1970-01-01', '{\"num\":0}'::jsonb"); !rs->End(); rs->MoveNext())
			{
				auto [i,j,f,s,n,t,nt,ts1,ts2,js] = rs->Row<s32_t,s32_t,double,TString,void,bool,bool,TTime,TTime,TJsonValue>();

				EXPECT_TRUE(i != nullptr && *i == 1);
				EXPECT_TRUE(j != nullptr && *j == 2);
				EXPECT_TRUE(f != nullptr && *f == 3.0);
				EXPECT_TRUE(s != nullptr && *s == "hello world");
				EXPECT_TRUE(n == nullptr);
				EXPECT_TRUE(t != nullptr && *t == true);
				EXPECT_TRUE(nt != nullptr && *nt == false);
				EXPECT_TRUE(ts1 != nullptr && *ts1 == 946684800);
				EXPECT_TRUE(ts2 != nullptr && *ts2 == 0);
				EXPECT_TRUE(js != nullptr && js->Map().Contains("num"));

				n_rows++;
			}
			EXPECT_EQ(n_rows, 1U);
		}

		{
			usys_t n_rows = 0;
			for(auto rs = connection.Execute("select oid from pg_type"); !rs->End(); rs->MoveNext())
			{
				auto [i] = rs->Row<s32_t>();
				EXPECT_TRUE(i != nullptr && *i != 0);
				n_rows++;
			}
			EXPECT_GT(n_rows, 100U);
		}
	}


	TEST(db_postgres, datatype_codecs)
	{
		TSortedMap<TString,TString> properties;

		{
			TPostgresConnection connection(properties);
			connection.Execute("DROP DOMAIN IF EXISTS public.el1_test_int4_domain CASCADE")->DiscardAllRows();
			connection.Execute("CREATE DOMAIN public.el1_test_int4_domain AS int4")->DiscardAllRows();
		}

		{
			TPostgresConnection connection(properties);

			auto rs = connection.Execute("SELECT 1234::public.el1_test_int4_domain, int4out(5678)");
			auto [domain_value, cstring_value] = rs->Row<s32_t,TString>();
			ASSERT_NE(domain_value, nullptr);
			ASSERT_NE(cstring_value, nullptr);
			EXPECT_EQ(*domain_value, 1234);
			EXPECT_TRUE(*cstring_value == "5678");

			rs = connection.Execute("SELECT pg_sleep(0)");
			auto [void_value] = rs->Row<void>();
			EXPECT_NE(void_value, nullptr);

			TList<byte_t> bytea_value = {0, 1, 2, 3, 255};
			rs = connection.Execute("SELECT $1::bytea", bytea_value);
			auto [bytea_result] = rs->Row<TList<byte_t>>();
			ASSERT_NE(bytea_result, nullptr);
			ASSERT_EQ(bytea_result->Count(), bytea_value.Count());
			for(usys_t i = 0; i < bytea_value.Count(); i++)
				EXPECT_EQ((*bytea_result)[i], bytea_value[i]);

			TList<byte_t> empty_bytea;
			rs = connection.Execute("SELECT $1::bytea", empty_bytea);
			auto [empty_bytea_result] = rs->Row<TList<byte_t>>();
			ASSERT_NE(empty_bytea_result, nullptr);
			EXPECT_EQ(empty_bytea_result->Count(), 0U);

			connection.Execute("DROP DOMAIN public.el1_test_int4_domain")->DiscardAllRows();
		}
	}

	TEST(db_postgres, ltree)
	{
		TSortedMap<TString,TString> properties;

		{
			TPostgresConnection connection(properties);
			try
			{
				connection.Execute("CREATE EXTENSION IF NOT EXISTS ltree")->DiscardAllRows();
			}
			catch(const TPostgresException& e)
			{
				GTEST_SKIP() << e.what();
			}
		}

		{
			TPostgresConnection connection(properties);
			using TGenericPath = el1::io::path::TPath;

			const TGenericPath value("Top.Science.Astronomy", '.');
			auto rs = connection.Execute("SELECT $1::ltree, subpath($1::ltree, 0, 2), ''::ltree", value);
			auto [full_path, parent_path, empty_path] = rs->Row<TGenericPath,TGenericPath,TGenericPath>();

			ASSERT_NE(full_path, nullptr);
			ASSERT_NE(parent_path, nullptr);
			ASSERT_NE(empty_path, nullptr);
			EXPECT_TRUE(full_path->ToString() == "Top.Science.Astronomy");
			EXPECT_TRUE(parent_path->ToString() == "Top.Science");
			EXPECT_TRUE(empty_path->IsEmpty());
			EXPECT_EQ(full_path->Separator(), el1::io::text::encoding::TUTF32('.'));
		}
	}

	TEST(db_postgres, notify_channel)
	{
		TSortedMap<TString,TString> properties;
		TPostgresConnection listener_connection(properties);
		TPostgresConnection sender_connection(properties);
		EXPECT_THROW(listener_connection.SubscribeNotifyChannel(""), TInvalidArgumentException);

		auto rs = listener_connection.Execute("SELECT pg_backend_pid()");
		auto [backend_pid] = rs->Row<s32_t>();
		ASSERT_NE(backend_pid, nullptr);

		const TString channel_name = TString::Format("el1 notify \"channel\" %d", *backend_pid);
		const TString first_payload = "first payload";
		const TString second_payload = "second payload";

		auto listener1 = listener_connection.SubscribeNotifyChannel(channel_name);
		auto listener2 = listener_connection.SubscribeNotifyChannel(channel_name);

		EXPECT_TRUE(listener1->Name() == channel_name);
		EXPECT_TRUE(listener2->Name() == channel_name);
		ASSERT_NE(listener1->OnNotify(), nullptr);
		ASSERT_NE(listener2->OnNotify(), nullptr);
		EXPECT_FALSE(listener1->OnNotify()->IsReady());
		EXPECT_FALSE(listener2->OnNotify()->IsReady());
		EXPECT_EQ(listener1->Read(), nullptr);
		EXPECT_EQ(listener2->Read(), nullptr);

		rs = listener_connection.Execute("SELECT EXISTS (SELECT 1 FROM pg_listening_channels() AS channel(name) WHERE name = $1)", channel_name);
		auto [is_listening] = rs->Row<bool>();
		ASSERT_NE(is_listening, nullptr);
		EXPECT_TRUE(*is_listening);

		sender_connection.Execute("SELECT pg_notify($1, $2)", channel_name, first_payload)->DiscardAllRows();
		ASSERT_TRUE(listener1->OnNotify()->WaitFor(5));
		ASSERT_TRUE(listener2->OnNotify()->WaitFor(5));

		auto event1 = listener1->Read();
		auto event2 = listener2->Read();
		ASSERT_NE(event1, nullptr);
		ASSERT_NE(event2, nullptr);
		EXPECT_TRUE(*event1 == first_payload);
		EXPECT_TRUE(*event2 == first_payload);
		EXPECT_EQ(listener1->Read(), nullptr);
		EXPECT_EQ(listener2->Read(), nullptr);
		EXPECT_FALSE(listener1->OnNotify()->IsReady());
		EXPECT_FALSE(listener2->OnNotify()->IsReady());

		listener1.reset();
		sender_connection.Execute("SELECT pg_notify($1, $2)", channel_name, second_payload)->DiscardAllRows();
		ASSERT_TRUE(listener2->OnNotify()->WaitFor(5));

		event2 = listener2->Read();
		ASSERT_NE(event2, nullptr);
		EXPECT_TRUE(*event2 == second_payload);
		EXPECT_EQ(listener2->Read(), nullptr);

		listener2.reset();
		rs = listener_connection.Execute("SELECT EXISTS (SELECT 1 FROM pg_listening_channels() AS channel(name) WHERE name = $1)", channel_name);
		std::tie(is_listening) = rs->Row<bool>();
		ASSERT_NE(is_listening, nullptr);
		EXPECT_FALSE(*is_listening);

		auto listener3 = listener_connection.SubscribeNotifyChannel(channel_name);
		const TString empty_payload;
		sender_connection.Execute("SELECT pg_notify($1, $2)", channel_name, empty_payload)->DiscardAllRows();
		ASSERT_TRUE(listener3->OnNotify()->WaitFor(5));

		auto event3 = listener3->Read();
		ASSERT_NE(event3, nullptr);
		EXPECT_TRUE(event3->Length() == 0);
		EXPECT_EQ(listener3->Read(), nullptr);
	}

	TEST(db_postgres, notify_channel_disconnect)
	{
		TSortedMap<TString,TString> properties;
		std::unique_ptr<TChannelListener> listener;

		{
			TPostgresConnection connection(properties);
			listener = connection.SubscribeNotifyChannel("el1_notify_disconnect_test");
			EXPECT_TRUE(listener->Name() == "el1_notify_disconnect_test");
			ASSERT_NE(listener->OnNotify(), nullptr);
		}

		EXPECT_TRUE(listener->Name().Length() == 0);
		EXPECT_EQ(listener->OnNotify(), nullptr);
		EXPECT_EQ(listener->Read(), nullptr);
		listener.reset();
	}

	TEST(db_postgres, wrong_column_count)
	{
		TSortedMap<TString,TString> properties;
		TPostgresConnection connection(properties);

		{
			usys_t n_rows = 0;
			bool _throw = false;
			for(auto rs = connection.Execute("select 1,2,3"); !rs->End(); rs->MoveNext())
			{
				try
				{
					auto [i,j] = rs->Row<s32_t, s32_t>();
					EXPECT_TRUE(i != nullptr && *i == 1);
					EXPECT_TRUE(j != nullptr && *j == 2);
				}
				catch(const IException&)
				{
					_throw = true;
				}
				n_rows++;
			}
			EXPECT_EQ(n_rows, 1U);
			EXPECT_TRUE(_throw);
		}

		{
			usys_t n_rows = 0;
			bool _throw = false;
			for(auto rs = connection.Execute("select 1"); !rs->End(); rs->MoveNext())
			{
				try
				{
					auto [i,j] = rs->Row<s32_t, s32_t>();
					EXPECT_TRUE(i != nullptr && *i == 1);
					EXPECT_TRUE(j != nullptr && *j == 2);
				}
				catch(const IException&)
				{
					_throw = true;
				}
				n_rows++;
			}
			EXPECT_EQ(n_rows, 1U);
			EXPECT_TRUE(_throw);
		}
	}

	TEST(db_postgres, bad_query_recovery)
	{
		TSortedMap<TString,TString> properties;
		TPostgresConnection connection(properties);

		auto rs = connection.Execute("some nonsense text");
		EXPECT_THROW(rs->DiscardAllRows(), TPostgresException);

		rs = connection.Execute("select 1234");
		EXPECT_EQ(rs->CountColumns(), 1U);
		auto [i] = rs->Row<s32_t>();
		EXPECT_TRUE(i != nullptr && *i == 1234);
	}

	TEST(db_postgres, pipeline)
	{
		TSortedMap<TString,TString> properties;
		TPostgresConnection connection(properties);

		auto rs1 = connection.Execute("select 10");
		auto rs2 = connection.Execute("select 20");
		auto rs3 = connection.Execute("select 30");

		usys_t n_rows = 0;
		for(; !rs1->End(); rs1->MoveNext())
		{
			auto [i] = rs1->Row<s32_t>();
			EXPECT_TRUE(i != nullptr && *i == 10);
			n_rows++;
		}
		EXPECT_EQ(n_rows, 1U);

		n_rows = 0;
		for(; !rs2->End(); rs2->MoveNext())
		{
			auto [i] = rs2->Row<s32_t>();
			EXPECT_TRUE(i != nullptr && *i == 20);
			n_rows++;
		}
		EXPECT_EQ(n_rows, 1U);

		n_rows = 0;
		for(; !rs3->End(); rs3->MoveNext())
		{
			auto [i] = rs3->Row<s32_t>();
			EXPECT_TRUE(i != nullptr && *i == 30);
			n_rows++;
		}
		EXPECT_EQ(n_rows, 1U);
 	}

 	TEST(db_postgres, TPostgresConnection_Prepare)
	{
		TSortedMap<TString,TString> properties;
		TPostgresConnection connection(properties);

		auto ps = connection.Prepare("SELECT 1234");
		auto rs = ps->Execute();
		EXPECT_EQ(rs->CountColumns(), 1U);
		auto [i] = rs->Row<s32_t>();
		EXPECT_TRUE(i != nullptr && *i == 1234);
	}
}
#endif
