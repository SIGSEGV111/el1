#ifdef EL1_WITH_POSTGRES
#include <gtest/gtest.h>
#include <el1/db_postgres.hpp>
#include <arpa/inet.h>

using namespace ::testing;

namespace
{
	using namespace el1::db::postgres;
	using namespace el1::io::text::string;
	using namespace el1::io::collection::list;
	using namespace el1::io::collection::map;
	using namespace el1::error;

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
			for(auto rs = connection.Execute("select 1,2,3.0::float8,'hello world',NULL,true,false"); !rs->End(); rs->MoveNext())
			{
				auto [i,j,f,s,n,t,nt] = rs->Row<s32_t,s32_t,double,TString,void,bool,bool>();

				EXPECT_TRUE(i != nullptr && *i == 1);
				EXPECT_TRUE(j != nullptr && *j == 2);
				EXPECT_TRUE(f != nullptr && *f == 3.0);
				EXPECT_TRUE(s != nullptr && *s == "hello world");
				EXPECT_TRUE(n == nullptr);
				EXPECT_TRUE(t != nullptr && *t == true);
				EXPECT_TRUE(nt != nullptr && *nt == false);

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
				catch(const TInvalidArgumentException&)
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
				catch(const TInvalidArgumentException&)
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
}
#endif
