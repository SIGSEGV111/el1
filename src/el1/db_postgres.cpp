#ifdef EL1_WITH_POSTGRES
#include "debug.hpp"
#include "db_postgres.hpp"
#include "io_collection_list.hpp"
#include "io_format_json.hpp"
#include "io_text_encoding_utf8.hpp"
#include "error.hpp"
#include <string.h>
#include <endian.h>
#include <libpq-fe.h>
#include <pgtypes_date.h>
#include <pgtypes_interval.h>
#include <pgtypes_timestamp.h>
#include <pgtypes_numeric.h>
#include <endian.h>
#include <atomic>

namespace el1::db::postgres
{
	using namespace io::format::json;
	using namespace io::collection::list;

	// static Oid LookupOidByType(const std::type_info& type);
	static usys_t LookupTypeInfoByOid(const Oid oid);
	static const oid_type_map_t* LookupTypeInfoByType(const std::type_info& type, const bool fallback_nullptr);

	static void DestructNoOp(void* const)
	{
	}

	static usys_t SerializeNoOp(const oid_type_map_t&, const void* const, void* const)
	{
		return 0;
	}

	static void DeserializeNoOp(const oid_type_map_t&, const void* const, void* const, const usys_t)
	{
	}

	template<typename T>
	static void Destruct(void* const value)
	{
		reinterpret_cast<T*>(value)->~T();
	}

	template<typename T>
	static usys_t SerializeCopy(const oid_type_map_t& ti, const void* const in, void* const out)
	{
		if(out)
			memcpy(out, in, sizeof(T));
		return sizeof(T);
	}

	template<typename T>
	static void DeserializeCopy(const oid_type_map_t& ti, const void* const in, void* const out, const usys_t sz_in)
	{
		if(out)
			memcpy(out, in, sizeof(T));
	}

	struct pq_array_t
	{
		s32_t n_dim;
		s32_t _unused;
		Oid item_oid;
		s32_t n_items;
		s32_t low_bound;
	};

	template<typename TItem>
	static usys_t SerializeArray(const oid_type_map_t& ti, const void* const in, void* const out)
	{
		auto& list = *(const TList<const TItem>*)in;
		const oid_type_map_t& ti_item = *LookupTypeInfoByType(typeid(TItem), false);
		EL_ERROR(list.Count() >= 2147483648U, TException, "list has too many elements");
		EL_ERROR(ti_item.sz_bytes <= 0, TInvalidArgumentException, "TItem", "items with variable size are not supported");

		if(out)
		{
			pq_array_t* arr = (pq_array_t*)out;
			arr->n_dim = 1;
			arr->_unused = 0;
			arr->item_oid = ti_item.oid;
			arr->n_items = (s32_t)list.Count();
			arr->low_bound = 0;

			byte_t* out_item = (byte_t*)(arr + 1);
			for(usys_t i = 0; i < list.Count(); i++)
			{
				ti_item.serialize(ti_item, &list[i], out_item);
				out_item += ti_item.sz_bytes;
			}
		}

		return sizeof(pq_array_t) + list.Count() * ti_item.sz_bytes;
	}

	template<typename TItem>
	static void DeserializeArray(const oid_type_map_t& ti, const void* const in, void* const out, const usys_t sz_in)
	{
		const oid_type_map_t& ti_item = *LookupTypeInfoByType(typeid(TItem), false);
		EL_ERROR(ti_item.sz_bytes <= 0, TInvalidArgumentException, "TItem", "items with variable size are not supported");

		pq_array_t* arr = (pq_array_t*)in;
		EL_ERROR(arr->n_dim != 1, TInvalidArgumentException, "in", "the input array must have exactly one dimension");
		EL_ERROR(arr->n_items < 0, TInvalidArgumentException, "in", "the input array has a negative item count");
		EL_ERROR(ti_item.oid != arr->item_oid, TInvalidArgumentException, "TItem", "the array elements have a different type than this function expects");

		if(out)
		{
			auto& list = *(new (out) TList<TItem>(arr->n_items));
			list.SetCount(arr->n_items);

			const byte_t* in_item = (byte_t*)(arr + 1);
			for(s32_t i = 0; i < arr->n_items; i++)
			{
				ti_item.deserialize(ti_item, in_item, &list[i], ti_item.sz_bytes);
				in_item += ti_item.sz_bytes;
			}
		}
	}

	template<typename T>
	static usys_t SerializeInteger(const oid_type_map_t& ti, const void* const in, void* const out)
	{
		if(out)
		{
			switch(sizeof(T))
			{
				case 1:
					memcpy(out, in, 1);
					break;
				case 2:
					*((s16_t*)out) = htobe16(*(const s16_t*)in);
					break;
				case 4:
					*((s32_t*)out) = htobe32(*(const s32_t*)in);
					break;
				case 8:
					*((s64_t*)out) = htobe64(*(const s64_t*)in);
					break;
				default:
					EL_THROW(TLogicException);
			}
		}

		return sizeof(T);
	}

	template<typename T>
	static void DeserializeInteger(const oid_type_map_t& ti, const void* const in, void* const out, const usys_t sz_in)
	{
		if(out)
		{
			switch(sizeof(T))
			{
				case 1:
					memcpy(out, in, 1);
					break;
				case 2:
					*((s16_t*)out) = be16toh(*(const s16_t*)in);
					break;
				case 4:
					*((s32_t*)out) = be32toh(*(const s32_t*)in);
					break;
				case 8:
					*((s64_t*)out) = be64toh(*(const s64_t*)in);
					break;
				default:
					EL_THROW(TLogicException);
			}
		}
	}

	template<typename T>
	static usys_t SerializeFloat(const oid_type_map_t& ti, const void* const in, void* const out)
	{
		if(out)
		{
			switch(sizeof(T))
			{
				case 4:
					*((s32_t*)out) = htobe32(*(const s32_t*)in);
					break;
				case 8:
					*((s64_t*)out) = htobe64(*(const s64_t*)in);
					break;
				default:
					EL_THROW(TLogicException);
			}
		}

		return sizeof(T);
	}

	template<typename T>
	static void DeserializeFloat(const oid_type_map_t& ti, const void* const in, void* const out, const usys_t sz_in)
	{
		if(out)
		{
			switch(sizeof(T))
			{
				case 4:
					*((s32_t*)out) = be32toh(*(const s32_t*)in);
					break;
				case 8:
					*((s64_t*)out) = be64toh(*(const s64_t*)in);
					break;
				default:
					EL_THROW(TLogicException);
			}
		}
	}

	static usys_t SerializeTimestamp(const oid_type_map_t& ti, const void* const in, void* const out)
	{
		// 64bit IEEE integer in µS since 2000-01-01 00:00:00 UTC (UNIXTIME: 946684800)
		// https://stackoverflow.com/questions/54678121/manually-convert-postgresql-binary-format-of-timestamp-to-integer
		EL_ERROR(ti.oid != 1114 && ti.oid != 1184, TNotImplementedException);

		if(out)
		{
			const TTime& ts = *reinterpret_cast<const TTime*>(in);
			s64_t& i = *reinterpret_cast<s64_t*>(out);
			i = htobe64(ts.ConvertToI(EUnit::MICROSECONDS) - 946684800000000LL);
		}

		return 8;;
	}

	static void DeserializeTimestamp(const oid_type_map_t& ti, const void* const in, void* const out, const usys_t sz_in)
	{
		// 64bit IEEE integer in µS since 2000-01-01 00:00:00 UTC (UNIXTIME: 946684800)
		// https://stackoverflow.com/questions/54678121/manually-convert-postgresql-binary-format-of-timestamp-to-integer
		EL_ERROR((ti.oid != 1114 && ti.oid != 1184) || sz_in != 8, TNotImplementedException);

		if(out)
		{
			const s64_t i = be64toh(*reinterpret_cast<const s64_t*>(in));
			TTime& ts = *reinterpret_cast<TTime*>(out);
			ts = TTime::ConvertFrom(EUnit::MICROSECONDS, i + 946684800000000LL);
		};
	}

	static usys_t SerializeString(const oid_type_map_t& ti, const void* const in, void* const out)
	{
		const TString& str = *(const TString*)in;

		if(out)
		{
			const usys_t len = str.chars.Pipe().Transform(io::text::encoding::utf8::TUTF8Encoder()).Read((byte_t*)out, str.Length() * 4U);
			return len;
		}
		else
		{
			return str.Length() * 4U;
		}
	}

	static void DeserializeString(const oid_type_map_t& ti, const void* const in, void* const out, const usys_t sz_in)
	{
		if(out)
		{
			new (out) TString((const char*)in, sz_in);
		}
	}

	static usys_t SerializeJson(const oid_type_map_t& ti, const void* const in, void* const out)
	{
		const TJsonValue& json = *(const TJsonValue*)in;
		TString str = json.ToString();
		// str.chars.Insert(0, TUTF32(1U));

		if(out)
		{
			const usys_t len = str.chars.Pipe().Transform(io::text::encoding::utf8::TUTF8Encoder()).Read((byte_t*)out, str.Length() * 4U);
			// debug::Hexdump("SerializeJson", out, len);
			return len;
		}
		else
		{
			return str.Length() * 4U;
		}
	}

	static void DeserializeJson(const oid_type_map_t& ti, const void* const in, void* const out, const usys_t sz_in)
	{
		// fprintf(stderr, "DeserializeJson: in = %p (\"%s\"), sz_in = %zu, out = %p\n", in, (const char*)in, (size_t)sz_in, out);
		// debug::Hexdump("DeserializeJson", in, sz_in);
		if(out)
		{
			TString s;
			if(ti.oid == 3802)
			{
				EL_ERROR(sz_in == 0, TLogicException);
				s = TString((const char*)in + 1, sz_in - 1);	// there is a 0x01 character at the start that obviously confuses the JSON parser - the purpose of that 0x01 is unknown
			}
			else
			{
				s = TString((const char*)in, sz_in);
			}

			new (out) TJsonValue(TJsonValue::Parse(s));
		}
	}

	const oid_type_map_t ARR_OID_TYPE_MAP[] = {
		{     16,  1, "bool"       , &typeid(bool      ), &SerializeCopy<bool    >, &DeserializeCopy<bool    >, &Destruct<bool      > },
		{     17, -1, "bytea"      , &typeid(byte_t    ), &SerializeArray<byte_t >, &DeserializeArray<byte_t >, &Destruct<byte_t    > },
		{     18,  1, "char"       , &typeid(char      ), &SerializeCopy<char    >, &DeserializeCopy<char    >, &Destruct<char      > },
		{     20,  8, "int8"       , &typeid(s64_t     ), &SerializeInteger<s64_t>, &DeserializeInteger<s64_t>, &Destruct<s64_t     > },
		{     21,  2, "int2"       , &typeid(s16_t     ), &SerializeInteger<s16_t>, &DeserializeInteger<s16_t>, &Destruct<s16_t     > },
		{     23,  4, "int4"       , &typeid(s32_t     ), &SerializeInteger<s32_t>, &DeserializeInteger<s32_t>, &Destruct<s32_t     > },
		{     25, -1, "text"       , &typeid(TString   ), &SerializeString        , &DeserializeString        , &Destruct<TString   > },
		{     26,  4, "oid"        , &typeid(s32_t     ), &SerializeInteger<s32_t>, &DeserializeInteger<s32_t>, &Destruct<s32_t     > },
		{    114, -1, "json"       , &typeid(TJsonValue), &SerializeJson          , &DeserializeJson          , &Destruct<TJsonValue> },
		{    142, -1, "xml"        , &typeid(TString   ), &SerializeString        , &DeserializeString        , &Destruct<TString   > },
		{    700,  4, "float4"     , &typeid(float     ), &SerializeFloat<float > , &DeserializeFloat<float > , &Destruct<float     > },
		{    701,  8, "float8"     , &typeid(double    ), &SerializeFloat<double> , &DeserializeFloat<double> , &Destruct<double    > },
		{    705,  0, "unknown"    , &typeid(void      ), &SerializeNoOp          , &DeserializeNoOp          , &DestructNoOp         },
		{   1043, -1, "varchar"    , &typeid(TString   ), &SerializeString        , &DeserializeString        , &Destruct<char      > },
		{   1114,  8, "timestamp"  , &typeid(TTime     ), &SerializeTimestamp     , &DeserializeTimestamp     , &Destruct<TTime     > },
		{   1184,  8, "timestamptz", &typeid(TTime     ), &SerializeTimestamp     , &DeserializeTimestamp     , &Destruct<TTime     > },
		{   1186, 16, "interval"   , &typeid(TTime     ), &SerializeTimestamp     , &DeserializeTimestamp     , &Destruct<TTime     > },
		{   1266, 12, "timetz"     , &typeid(TTime     ), &SerializeTimestamp     , &DeserializeTimestamp     , &Destruct<TTime     > },
		{   3802, -1, "jsonb"      , &typeid(TJsonValue), &SerializeJson          , &DeserializeJson          , &Destruct<TJsonValue> },
		{ 222755, -2, "cstring"    , &typeid(TString   ), &SerializeString        , &DeserializeString        , &Destruct<TString   > },
	};

	const usys_t N_OID_TYPE_MAP = sizeof(ARR_OID_TYPE_MAP) / sizeof(ARR_OID_TYPE_MAP[0]);

	static usys_t LookupTypeInfoByOid(const Oid oid)
	{
		for(usys_t i = 0; i < N_OID_TYPE_MAP; i++)
			if(ARR_OID_TYPE_MAP[i].oid == oid)
				return i;
		EL_THROW(TException, TString::Format("OID %d not mapped to C++ datatype", oid));
	}

	static const oid_type_map_t* LookupTypeInfoByType(const std::type_info& type, const bool fallback_nullptr)
	{
		for(usys_t i = 0; i < N_OID_TYPE_MAP; i++)
			if(*ARR_OID_TYPE_MAP[i].type == type)
				return ARR_OID_TYPE_MAP + i;
		EL_ERROR(!fallback_nullptr, TException, TString::Format("C++ datatype %q not mapped to postgres OID", debug::Demangle(type.name())));
		return nullptr;
	}

	/**********************************************************************************/

	TString TPostgresException::Message() const
	{
		return message;
	}

	IException* TPostgresException::Clone() const
	{
		return new TPostgresException(*this);
	}

	TPostgresException::TPostgresException(void* pg_connection, const TString& description) : message(TString::Format("%s: %s", description, PQerrorMessage((PGconn*)pg_connection)))
	{
	}

	TPostgresException::TPostgresException(PGresult* pg_result, const TString& description) : message(TString::Format("%s: %s", description, PQresultErrorMessage(pg_result)))
	{
	}

	/**********************************************************************************/

	TPostgresColumnDescription::~TPostgresColumnDescription()
	{
		if(!is_null)
		{
			auto& ti = TypeInfo();
			ti.destruct(buffer);
			is_null = true;
		}
	}

	/**********************************************************************************/

	const system::waitable::IWaitable* TResultStream::OnNewData() const
	{
		return fifo.OnInputReady();
	}

	TString TResultStream::SQL() const
	{
		return sql;
	}

	bool TResultStream::IsMetadataReady() const
	{
		return columns.Count() > 0;
	}

	bool TResultStream::IsFirstRowReady() const
	{
		return columns.Count() > 0;
	}

	bool TResultStream::IsNextRowReady() const
	{
		return fifo.Remaining() > 0;
	}

	IDatabaseConnection* TResultStream::Connection() const
	{
		return conn;
	}

	bool TResultStream::End() const
	{
		if(!IsMetadataReady())
			const_cast<TResultStream*>(this)->MoveFirst();
		return fifo.Remaining() == 0 && fifo.OnInputReady() == nullptr;
	}

	usys_t TResultStream::CountColumns() const
	{
		if(!IsMetadataReady())
			const_cast<TResultStream*>(this)->MoveFirst();
		return columns.Count();
	}

	const TPostgresColumnDescription& TResultStream::Column(const usys_t index) const
	{
		if(!IsMetadataReady())
			const_cast<TResultStream*>(this)->MoveFirst();
		return columns[index];
	}

	const void* TResultStream::Cell(const usys_t index) const
	{
		if(!IsFirstRowReady())
			const_cast<TResultStream*>(this)->MoveFirst();

		EL_ERROR(index >= columns.Count(), TIndexOutOfBoundsException, 0, columns.Count() - 1, index);
		const TPostgresColumnDescription& d = columns[index];
		return d.is_null ? nullptr : d.buffer;
	}

	void TResultStream::DiscardAllRows()
	{
		while(!End())
			InternalMoveNext();
	}

	void TResultStream::MoveFirst()
	{
		EL_ERROR(columns.Count() != 0, TLogicException);
		auto current_result = InternalMoveNext();

		const int n_fields = PQnfields(current_result.get());
		for(int i = 0; i < n_fields; i++)
		{
			TPostgresColumnDescription d;
			d.name = PQfname(current_result.get(), i);
			d.idx_typemap = LookupTypeInfoByOid(PQftype(current_result.get(), i));
			d.type = ARR_OID_TYPE_MAP[d.idx_typemap].type;
			d.is_null = true;
			memset(d.buffer, 0, sizeof(d.buffer));
			columns.MoveAppend(std::move(d));
		}

		DeserializeResult(std::move(current_result));
	}

	result_ptr_t TResultStream::InternalMoveNext()
	{
		result_ptr_t current_result(nullptr, &PQclear);
		void* tmp;

		for(;;)
		{
			while(fifo.Remaining() == 0)
			{
				auto w = OnNewData();
				if(w == nullptr)
				{
					// EOF
					fifo.CloseInput();
					return result_ptr_t(nullptr, &PQclear);
				}
				w->WaitFor();
			}

			EL_ERROR(fifo.Read(&tmp, 1) != 1, TLogicException);
			EL_ERROR(tmp == nullptr, TLogicException);
			current_result = result_ptr_t((PGresult*)tmp, &PQclear);

			const ExecStatusType status = PQresultStatus(current_result.get());
			switch(status)
			{
				case PGRES_COMMAND_OK:
					break;

				case PGRES_TUPLES_OK:
					// last result of this query to signal successful execution - it should not contain any rows
					EL_ERROR(PQntuples(current_result.get()) != 0, TLogicException);
					break;

				case PGRES_SINGLE_TUPLE:
					return current_result;

				// error handling...
				case PGRES_EMPTY_QUERY:
					EL_THROW(TException, "empty query");
				case PGRES_COPY_OUT:
					EL_THROW(TLogicException);
				case PGRES_COPY_IN:
					EL_THROW(TLogicException);
				case PGRES_BAD_RESPONSE:
					EL_THROW(TLogicException);
				case PGRES_NONFATAL_ERROR:
					EL_THROW(TPostgresException, current_result.get(), "PGRES_NONFATAL_ERROR");
				case PGRES_FATAL_ERROR:
					EL_THROW(TPostgresException, current_result.get(), "PGRES_FATAL_ERROR");
				case PGRES_COPY_BOTH:
					EL_THROW(TLogicException);
				case PGRES_PIPELINE_SYNC:
					EL_THROW(TLogicException);
				case PGRES_PIPELINE_ABORTED:
					EL_THROW(TPostgresException, current_result.get(), "PGRES_PIPELINE_ABORTED");
			}
		}
	}

	void TResultStream::DeserializeResult(result_ptr_t current_result)
	{
		for(usys_t i = 0; i < columns.Count(); i++)
		{
			TPostgresColumnDescription& d = columns[i];
			auto& ti = d.TypeInfo();

			if(!d.is_null)
			{
				ti.destruct(d.buffer);
				d.is_null = true;
			}

			if(current_result != nullptr && PQgetisnull(current_result.get(), 0, i) == 0)
			{
				const void* value = PQgetvalue(current_result.get(), 0, i);
				EL_ERROR(value == nullptr, TLogicException);
				EL_ERROR(ti.sz_bytes > (ssize_t)sizeof(d.buffer), TLogicException);
				ti.deserialize(ti, value, d.buffer, PQgetlength(current_result.get(), 0, i));
				d.is_null = false;
			}
		}
	}

	void TResultStream::MoveNext()
	{
		DeserializeResult(InternalMoveNext());
	}

	TResultStream::TResultStream(TPostgresConnection* const conn, TString sql) : conn(conn), sql(std::move(sql))
	{
	}

	TResultStream::~TResultStream()
	{
		if(conn != nullptr)
		{
			for(auto& q : conn->active_queries)
				if(q == this)
				{
					q = nullptr;
					break;
				}
		}

		void* r = nullptr;
		while(fifo.Read(&r, 1) == 1)
			PQclear((PGresult*)r);
	}

	/**********************************************************************************/

	static TString GenerateUniqueStatementName()
	{
		static std::atomic_uint i = 0;
		i++;
		return TString::Format("st_%d", (unsigned)i);
	}

	TString TStatement::SQL() const
	{
		return sql;
	}

	std::unique_ptr<IResultStream> TStatement::Execute(array_t<query_arg_t> args)
	{
		TString exec = "EXECUTE ";
		exec += name;
		if(args.Count() > 0)
		{
			exec += "($1";
			for(usys_t i = 1; i < args.Count(); i++)
				exec += TString::Format(",$%d", i);
			exec += ")";
		}
		return conn->Execute(exec, args);
	}

	TStatement::TStatement(TPostgresConnection* const conn, TString sql) : conn(conn), name(GenerateUniqueStatementName()), sql(sql)
	{
		conn->Execute(TString::Format("PREPARE %s AS %s", name, sql))->DiscardAllRows();
	}

	TStatement::~TStatement()
	{
		conn->Execute(TString::Format("DEALLOCATE %s", name))->DiscardAllRows();
	}

	/**********************************************************************************/

	TString TChannelListener::Name() const
	{
		if(channel->conn != nullptr)
		{
			for(const auto& pair : channel->conn->notify_channels.Items())
				if(pair.value == channel)
					return pair.key;

			EL_THROW(TLogicException);
		}

		return "";
	}

	system::waitable::TMemoryWaitable<usys_t>* TChannelListener::OnNotify() const
	{
		return channel->conn != nullptr ? &on_pending_event : nullptr;
	}

	std::shared_ptr<const TString> TChannelListener::Read()
	{
		if(pending_events.Count() == 0)
			return nullptr;

		std::shared_ptr<const TString> msg = std::move(pending_events[0]);
		pending_events.Remove(0, 1U);

		return msg;
	}

	TChannelListener::TChannelListener(std::shared_ptr<TNotifyChannel> channel) : channel(std::move(channel)), pending_events(), on_pending_event(pending_events.MakeItemCountWaitable(nullptr))
	{
		this->channel->listeners.Append(this);
	}

	TChannelListener::~TChannelListener()
	{
		channel->listeners.RemoveItem(this);
		if(channel->listeners.Count() == 0 && channel->conn != nullptr)
			channel->conn->ShutdownNotifyChannel(Name());
	}

	/**********************************************************************************/

	void TPostgresConnection::FlusherMain()
	{
		try
		{
			system::task::THandleWaitable on_tx_ready({.read=false,.write=true,.other=false}, PQsocket((PGconn*)pg_connection));
			for(;;)
			{
				const int r = PQflush((PGconn*)pg_connection);
				EL_ERROR(r < 0, TPostgresException, pg_connection, "unable to flush output buffer to server");
				if(r > 0) // more data to send but unable at the moment
					on_tx_ready.WaitFor();
				else // no more data to send (r == 0)
					TFiber::Self()->Stop();
			}
		}
		catch(shutdown_t)
		{
		}
		catch(...)
		{
			Disconnect();
			throw;
		}
	}

	void TPostgresConnection::ReaderMain()
	{
		try
		{
			system::task::THandleWaitable on_rx_ready({.read=true,.write=false,.other=true}, PQsocket((PGconn*)pg_connection));
			for(;;)
			{
				on_rx_ready.WaitFor();
				EL_ERROR(PQconsumeInput((PGconn*)pg_connection) == 0, TPostgresException, pg_connection, "unable to process data from server server");

				gt_begin:;
				while(active_queries.Count() > 0 && PQisBusy((PGconn*)pg_connection) == 0)
				{
					void* const result = PQgetResult((PGconn*)pg_connection);
					PQsetSingleRowMode((PGconn*)pg_connection);
					TResultStream* const rs = active_queries[0];

					if(EL_UNLIKELY(result == nullptr))
					{
						// end of result stream reached
						active_queries.Remove(0, 1U);
						if(EL_LIKELY(rs != nullptr))
						{
							rs->fifo.CloseOutput();
							rs->conn = nullptr;
						}
					}
					else
					{
						const ExecStatusType status = PQresultStatus((PGresult*)result);

						switch(status)
						{
							case PGRES_EMPTY_QUERY:
								break;
							case PGRES_COMMAND_OK:
								break;
							case PGRES_TUPLES_OK:
								break;
							case PGRES_COPY_OUT:
								EL_NOT_IMPLEMENTED;
							case PGRES_COPY_IN:
								EL_NOT_IMPLEMENTED;
							case PGRES_BAD_RESPONSE:
								EL_THROW(TLogicException);
							case PGRES_NONFATAL_ERROR:
								break;
							case PGRES_FATAL_ERROR:
								break;
							case PGRES_COPY_BOTH:
								EL_THROW(TLogicException);
							case PGRES_SINGLE_TUPLE:
								break;
							case PGRES_PIPELINE_SYNC:
								PQclear((PGresult*)result);
								goto gt_begin;
							case PGRES_PIPELINE_ABORTED:
								rs->fifo.WriteAll(&result, 1U);
								rs->fifo.CloseOutput();
								rs->conn = nullptr;
								active_queries.Remove(0, 1U);
								goto gt_begin;
						}

						if(EL_UNLIKELY(rs == nullptr))
						{
							// client lost interest in the query and closed the result stream => discard result
							PQclear((PGresult*)result);
						}
						else
						{
							// hand over result
							rs->fifo.WriteAll(&result, 1U);
						}
					}
				}

				while(PGnotify* event = PQnotifies((PGconn*)pg_connection))
				{
					const TString channel_name = event->relname;
					auto payload = std::shared_ptr<const TString>(new TString(event->extra));
					PQfreemem(event);

					std::shared_ptr<TNotifyChannel>* channel = notify_channels.Get(channel_name);

					// It might happen that all listeners went away and there was still an event in the queue from
					// the server. Thus we might end up not having a TNotifyChannel any more but still received an event for it
					if(channel != nullptr)
					{
						for(auto l : (*channel)->listeners)
							l->pending_events.Append(payload);
					}
				}
			}
		}
		catch(shutdown_t)
		{
		}
		catch(...)
		{
			Disconnect();
			throw;
		}
	}

	TPostgresConnection::TPostgresConnection() : pg_connection(nullptr)
	{
	}

	TPostgresConnection::TPostgresConnection(const TSortedMap<TString, const TString>& properties) : TPostgresConnection()
	{
		Connect(properties);
	}

	void TPostgresConnection::Connect(const TSortedMap<TString, const TString>& properties)
	{
		EL_ERROR(pg_connection != nullptr, TException, "already connected");

		TList<std::unique_ptr<char[]>> keywords;
		TList<std::unique_ptr<char[]>> values;

		for(const auto& pair : properties.Items())
		{
			keywords.MoveAppend(pair.key.MakeCStr());
			values.MoveAppend(pair.value.MakeCStr());
		}

		keywords.MoveAppend(std::unique_ptr<char[]>(nullptr));
		values.MoveAppend(std::unique_ptr<char[]>(nullptr));

		EL_ERROR((pg_connection = PQconnectStartParams((char**)&keywords[0], (char**)&values[0], 0)) == nullptr, TException, "libpq has been unable to allocate a new PGconn structure");

		PostgresPollingStatusType ps;
		do
		{
			ps = PQconnectPoll((PGconn*)pg_connection);
			switch(ps)
			{
				case PGRES_POLLING_READING:
				{
					system::task::THandleWaitable on_rx_ready({.read=true,.write=false,.other=false}, PQsocket((PGconn*)pg_connection));
					on_rx_ready.WaitFor();
					break;
				}
				case PGRES_POLLING_WRITING:
				{
					system::task::THandleWaitable on_tx_ready({.read=false,.write=true,.other=false}, PQsocket((PGconn*)pg_connection));
					on_tx_ready.WaitFor();
					break;
				}
				case PGRES_POLLING_FAILED:
					EL_THROW(TPostgresException, pg_connection, "unable to establish connection with server");

				case PGRES_POLLING_OK:
					break;

				case PGRES_POLLING_ACTIVE:
					// unused, see https://www.postgresql.org/message-id/20030226163108.GA1355@gnu.org
					EL_THROW(TLogicException);
			}
		}
		while(ps != PGRES_POLLING_OK);

		EL_ERROR(PQenterPipelineMode((PGconn*)pg_connection) != 1, TPostgresException, pg_connection, "unable to enter pipeline mode on postgres connection");
		EL_ERROR(PQsetnonblocking((PGconn*)pg_connection, 1) != 0, TPostgresException, pg_connection, "unable to set non-blocking mode on postgres connection");

		fib_flusher.Start(TFunction(this, &TPostgresConnection::FlusherMain));
		fib_reader.Start(TFunction(this, &TPostgresConnection::ReaderMain));
	}

	void TPostgresConnection::Disconnect()
	{
		for(auto pair : notify_channels.Items())
			pair.value->conn = nullptr;

		for(auto r : active_queries)
			if(r != nullptr)
			{
				r->fifo.CloseOutput();
				r->conn = nullptr;
			}

		active_queries.Clear();
		notify_channels.Clear();

		if(TFiber::Self() != &fib_flusher)
		{
			fib_flusher.Shutdown();
			if(auto e = fib_flusher.Join())
				e->Print("FlusherMain()");
		}

		if(TFiber::Self() != &fib_reader)
		{
			fib_reader.Shutdown();
			if(auto e = fib_reader.Join())
				e->Print("ReaderMain()");
		}

		if(pg_connection != nullptr)
			PQfinish((PGconn*)pg_connection);

		pg_connection = nullptr;
	}

	TPostgresConnection::~TPostgresConnection()
	{
		Disconnect();
	}

	void TPostgresConnection::StartNotifyChannel(const TString& channel_name)
	{
		Execute(TString::Format("LISTEN %s", channel_name))->DiscardAllRows();
	}

	void TPostgresConnection::ShutdownNotifyChannel(const TString& channel_name)
	{
		Execute(TString::Format("UNLISTEN %s", channel_name))->DiscardAllRows();
	}

	std::unique_ptr<IStatement> TPostgresConnection::Prepare(const TString& sql)
	{
		return New<TStatement>(this, sql);
	}

	std::unique_ptr<IResultStream> TPostgresConnection::Execute(const TString& sql, array_t<query_arg_t> args)
	{
		Oid oids[args.Count()];
		TList<byte_t> values[args.Count()];
		void* value_ptrs[args.Count()];
		int lengths[args.Count()];
		int formats[args.Count()];

		for(usys_t i = 0; i < args.Count(); i++)
		{
			const auto& a = args[i];

			EL_ERROR(a.value != nullptr && a.type == nullptr, TInvalidArgumentException, "args", "type must not be NULL when value is set");

			if(a.value != nullptr)
			{
				const oid_type_map_t& ti = *LookupTypeInfoByType(*a.type, false);
				oids[i] = ti.oid;
				lengths[i] = ti.serialize(ti, a.value, nullptr);
				values[i].SetCount(lengths[i]);
				value_ptrs[i] = &values[i][0];
				lengths[i] = ti.serialize(ti, a.value, value_ptrs[i]);
				formats[i] = 1;
			}
			else
			{
				oids[i] = 0;
				value_ptrs[i] = nullptr;
				lengths[i] = 0;
				formats[i] = 1;
			}
		}

		EL_ERROR(PQsendQueryParams((PGconn*)pg_connection, sql.MakeCStr().get(), args.Count(), &oids[0], (char**)&value_ptrs[0], &lengths[0], &formats[0], 1) != 1, TPostgresException, pg_connection, "unable to dispatch query to server");

		PQsetSingleRowMode((PGconn*)pg_connection);	// sometimes it works, sometimes it doesn't - after consulting the libPQ source it depends on whether or not the connection has an active result pending. Now we can't know this here, since we are working in a pipeline and we are here on the sending side, not the receiving side, so we do not know in what state the result processing is. So by trial-and-error I found that the first query needs it here, while further queries need it in ReaderMain()... and that seems to work... for now.

		EL_ERROR(PQpipelineSync((PGconn*)pg_connection) != 1, TPostgresException, pg_connection, "unable to send sync request");

		fib_flusher.Resume();

		auto rs = std::unique_ptr<TResultStream>(new TResultStream(this, sql));
		active_queries.Append(rs.get());
		return rs;
	}

	std::unique_ptr<TChannelListener> TPostgresConnection::SubscribeNotifyChannel(const TString& channel_name)
	{
		std::shared_ptr<TNotifyChannel>* channel = notify_channels.Get(channel_name);
		if(channel == nullptr)
		{
			channel = &notify_channels.Add(channel_name, std::shared_ptr<TNotifyChannel>(new TNotifyChannel(this)));
			StartNotifyChannel(channel_name);
		}

		auto l = std::unique_ptr<TChannelListener>(new TChannelListener(*channel));
		(*channel)->listeners.Append(l.get());
		return l;
	}
}
#endif
