#pragma once

#ifdef EL1_WITH_POSTGRES
#include "io_collection_list.hpp"
#include "io_collection_map.hpp"
#include "io_text_string.hpp"
#include "system_task.hpp"

namespace el1::db::postgres
{
	using namespace io::text::string;
	using namespace io::collection::list;
	using namespace io::collection::map;
	using namespace system::task;

	struct TPostgresException : IException
	{
		const TString message;

		TString Message() const final override;
		IException* Clone() const override;

		TPostgresException(void* pg_connection, const TString& description);
	};

	class TConnection;

	template<typename ... TColumns>
	class TResultSetStream
	{
		protected:
			TConnection* const connection;
			usys_t n_resultsets;
			std::tuple<TColumns ...> tuple;

		public:
			using TOut = std::tuple<TColumns* ...>;

			TOut* NextItem();

			TResultSetStream(TConnection* const connection, const usys_t n_resultsets);
	};

	using oid_t = unsigned int;

	struct data_t
	{
		oid_t oid;
		unsigned int length;
		const void* data;
	};

	struct type_key_t
	{
		const char* tid;	// C++ typeid(T)
		oid_t oid;			// postgres datatype OID

		bool operator!=(const type_key_t&) const EL_GETTER;
		bool operator==(const type_key_t&) const EL_GETTER;
		bool operator> (const type_key_t&) const EL_GETTER;
		bool operator< (const type_key_t&) const EL_GETTER;
	};

	using serialize_f = void (*)(const void* const object, TList<byte_t>& data);
	using deserialize_f = void (*)(const array_t<const byte_t> data, void* const object);

	struct type_convert_t
	{
		// converts a C++ value into pq binary format
		// can be nullptr, in this case it is assumed the that types are binary compatible and no conversion is needed
		serialize_f serialize;

		// overrides an existing instance of the C++ datatype with the provided pq binary data
		// can be nullptr, in this case it is assumed the that types are binary compatible and no conversion is needed
		deserialize_f deserialize;
	};

	// this map lists for each pair of C++ datatype and postgres datatype a conversion routine
	extern TSortedMap<type_key_t, type_convert_t> typemap;

	// this map holds the postgres datatype that is used to transmit a certain C++ type
	// key: typeid(T), value: postgres datatype OID
	// if a match is found the resulting pair of C++ type and postgres type is used as key in the typemap above
	// in order to find a conversion routine
	extern TSortedMap<const char*, oid_t> sendtype;

	class TConnection
	{
		protected:
			system::task::THandleWaitable on_rx_ready;
			system::task::THandleWaitable on_tx_ready;
			void* pg_connection;
			void* pg_result;
			int n_rows_cached;
			int idx_row;

			TFiber fiber_flusher;
			void FlusherMain();

			template<typename TColumn>
			void _ReadCellData(TColumn*& typed_cell);

			template<typename TColumn, typename ... TColumns>
			void _ReadCellData(array_t<data_t> raw_cells, const unsigned index, TColumn*& typed_cell, TColumns*& ... typed_cells);

			template<typename TColumn>
			void _ReadCellData(array_t<data_t> raw_cells, const unsigned index, TColumn*& typed_cell);

		public:
			TConnection(TConnection&&) = delete;
			TConnection(const TConnection&) = delete;
			TConnection(const TSortedMap<TString, const TString>& properties);
			~TConnection();

			template<typename ... TArgs>
			void Prepare(const TString& name, const TString& sql);
			void _Prepare(const TString& name, const TString& sql, const array_t<oid_t> param_oids);

			template<typename ... TArgs>
			void Execute(const TString& sql, TArgs ... args);
			void Execute(const TString& sql) { _Execute(sql); }
			void _Execute(const TString& sql, const array_t<const data_t> args = array_t<const data_t>());

			// returns the number of rows in the result
			// FIXME: this function might return a wrong count due to pipelining and single row mode
			unsigned CountRows() const EL_GETTER;

			// returns the number of columns in the result
			unsigned CountColumns() const EL_GETTER;

			// returns the index of the named column
			unsigned ColumnNameToIndex(const TString& name) const EL_GETTER;

			// fetches the next row and moves the iterator to it
			// return true if the next row was fetched or false if there are no more rows in this resultset
			// this function will block if there is outstanding data
			bool FetchNextRow();

			// alters the pointers passed as arguments to point to the values of the fields in the current row
			// database NULL values will result in the respecitive pointer to be set to nullptr
			// the memory is owned by the TConnection and must not be freed manually
			// the pointers and their values remain valid until the next row is fetched or the results are discarded
			// if the type of the pointer does not match with the type of the column an exception will be thrown
			template<typename ... TColumns>
			void ReadCellData(TColumns*& ... cells);

			// returns the cells of the current row untranslated
			void _ReadCellData(const array_t<data_t> cells);

			// discards any pending results left to read
			void DiscardResults();

			// streams the rows of the result set
			// if multiple statements were executed before with the same column layout they can be streamed in one go
			// use n_resultsets to specify from how many previously executed statements you want to stream the results
			// the results are streamed oldest to newest
			template<typename ... TColumns>
			TResultSetStream<TColumns ...> Stream(const usys_t n_resultsets = 1U);
	};

	template<typename ... TColumns>
	void TConnection::ReadCellData(TColumns*& ... typed_cells)
	{
		data_t arr_raw_cells[CountColumns()];
		array_t raw_cells(arr_raw_cells, CountColumns());
		_ReadCellData(raw_cells);
		_ReadCellData(raw_cells, 0, typed_cells ...);
	}

	template<typename TColumn>
	void TConnection::_ReadCellData(TColumn*& typed_cell)
	{
		data_t arr_raw_cells[CountColumns()];
		array_t raw_cells(arr_raw_cells, CountColumns());
		_ReadCellData(raw_cells);
		_ReadCellData(raw_cells, 0, typed_cell);
	}

	template<typename TColumn, typename ... TColumns>
	void TConnection::_ReadCellData(array_t<data_t> raw_cells, const unsigned index, TColumn*& typed_cell, TColumns*& ... typed_cells)
	{
		EL_ERROR(index >= raw_cells.Count(), TInvalidArgumentException, "cells", "resultset has less columns than arguments passed to to ReadCellData()");
		_ReadCellData(raw_cells, index, typed_cell);
		_ReadCellData(raw_cells, index + 1, typed_cells ...);
	}

	template<typename TColumn>
	void TConnection::_ReadCellData(array_t<data_t> raw_cells, const unsigned index, TColumn*& typed_cell)
	{
		const data_t& raw_cell = raw_cells[index];
		typed_cell = (TColumn*)raw_cell.data;
		EL_ERROR(raw_cell.oid != 25 && raw_cell.data != nullptr && raw_cell.length != sizeof(TColumn), TInvalidArgumentException, "cells", "datatype/size mismatch");
	}
}
#endif
