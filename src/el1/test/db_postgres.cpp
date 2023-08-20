#ifdef EL1_WITH_POSTGRES
#include <gtest/gtest.h>
#include <el1/db_postgres.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::db::postgres;
	using namespace el1::io::text::string;
	using namespace el1::io::collection::list;
	using namespace el1::io::collection::map;
	using namespace el1::error;

	TEST(db_postgres, TConnection_connect)
	{
		TSortedMap<TString,TString> properties;
		TConnection connection(properties);
	}

	TEST(db_postgres, TConnection_simple_select)
	{
		TSortedMap<TString,TString> properties;
		TConnection connection(properties);

		{
			connection.Execute("select cast(12345 as int4)");

			connection.FetchNextRow();
			EXPECT_EQ(connection.CountRows(), 1U);
			EXPECT_EQ(connection.CountColumns(), 1U);

			TList<data_t> cells = {{}};
			connection._ReadCellData(cells);

// 			std::cout<<"OID: "<<cells[0].oid<<"\n";
// 			std::cout<<"Length: "<<cells[0].length<<"\n";
// 			std::cout<<"Data: "<<cells[0].data<<"\n";

			int* column1 = nullptr;
			connection.ReadCellData(column1);
			EXPECT_NE(column1, nullptr);
			EXPECT_EQ(*column1, 12345);
			connection.DiscardResults();
		}

		{
			connection.Execute("select cast(17.4 as float8)");

			connection.FetchNextRow();
			EXPECT_EQ(connection.CountRows(), 1U);
			EXPECT_EQ(connection.CountColumns(), 1U);

			TList<data_t> cells = {{}};
			connection._ReadCellData(cells);

			double* column1 = nullptr;
			connection.ReadCellData(column1);
			EXPECT_NE(column1, nullptr);
			EXPECT_EQ(*column1, 17.4);
			connection.DiscardResults();
		}

		{
			connection.Execute("select cast(17.4 as float4)");
			connection.FetchNextRow();
			EXPECT_EQ(connection.CountRows(), 1U);
			EXPECT_EQ(connection.CountColumns(), 1U);

			TList<data_t> cells = {{}};
			connection._ReadCellData(cells);

			float* column1 = nullptr;
			connection.ReadCellData(column1);
			EXPECT_NE(column1, nullptr);
			EXPECT_EQ(*column1, 17.4f);
			connection.DiscardResults();
		}

		{
			connection.Execute("select 'hello world'");

			connection.FetchNextRow();
			EXPECT_EQ(connection.CountRows(), 1U);
			EXPECT_EQ(connection.CountColumns(), 1U);

			TList<data_t> cells = {{}};
			connection._ReadCellData(cells);

// 			std::cout<<"OID: "<<cells[0].oid<<"\n";
// 			std::cout<<"Length: "<<cells[0].length<<"\n";
// 			std::cout<<"Data: "<<cells[0].data<<"\n";

			const char* column1 = nullptr;
			connection.ReadCellData(column1);
			EXPECT_NE(column1, nullptr);
			EXPECT_TRUE(strcmp(column1, "hello world") == 0);
			connection.DiscardResults();
		}
	}

	TEST(db_postgres, TConnection_bad_query_recovery)
	{
		TSortedMap<TString,TString> properties;
		TConnection connection(properties);

		connection.Execute("some nonsense text");
		EXPECT_THROW(connection.FetchNextRow(), TPostgresException);
		connection.DiscardResults();

		connection.Execute("select 1");
		EXPECT_TRUE(connection.FetchNextRow());
		connection.DiscardResults();
	}

	TEST(db_postgres, TConnection_pipeline)
	{
		TSortedMap<TString,TString> properties;
		TConnection connection(properties);

		connection.Execute("create table test(id int not null, some_text text not null)");
		connection.Execute("insert into test values (1, 'foobar')");
		connection.Execute("insert into test values (2, 'blub')");
		connection.Execute("insert into test values (3, 'hello world')");
		connection.Execute("select * from test order by id");
		connection.Execute("drop table test");

		EXPECT_TRUE(connection.FetchNextRow());  // create table
		EXPECT_FALSE(connection.FetchNextRow()); // create table

		EXPECT_TRUE(connection.FetchNextRow());  // insert1
		EXPECT_FALSE(connection.FetchNextRow()); // insert1

		EXPECT_TRUE(connection.FetchNextRow());  // insert2
		EXPECT_FALSE(connection.FetchNextRow()); // insert2

		EXPECT_TRUE(connection.FetchNextRow());  // insert3
		EXPECT_FALSE(connection.FetchNextRow()); // insert3

		int* id;
		const char* str;

		EXPECT_TRUE(connection.FetchNextRow());
		connection.ReadCellData(id, str);
		EXPECT_EQ(*id, 1);
		EXPECT_TRUE(strcmp(str, "foobar") == 0);

		EXPECT_TRUE(connection.FetchNextRow());
		connection.ReadCellData(id, str);
		EXPECT_EQ(*id, 2);
		EXPECT_TRUE(strcmp(str, "blub") == 0);

		EXPECT_TRUE(connection.FetchNextRow());
		connection.ReadCellData(id, str);
		EXPECT_EQ(*id, 3);
		EXPECT_TRUE(strcmp(str, "hello world") == 0);

		EXPECT_FALSE(connection.FetchNextRow());

		EXPECT_TRUE(connection.FetchNextRow());  // drop table
		EXPECT_FALSE(connection.FetchNextRow()); // drop table
	}

	TEST(db_postgres, TConnection_empty_query)
	{
		TSortedMap<TString,TString> properties;
		TConnection connection(properties);

		connection.Execute("");

		EXPECT_TRUE(connection.FetchNextRow());
		EXPECT_FALSE(connection.FetchNextRow());
	}
}
#endif
