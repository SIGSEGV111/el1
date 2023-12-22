#pragma once

#ifdef EL1_WITH_POSTGRES
#include "io_collection_list.hpp"
#include "io_collection_map.hpp"
#include "io_text_string.hpp"
#include "io_stream_fifo.hpp"
#include "system_task.hpp"
#include "util_function.hpp"
#include "util_event.hpp"
#include "db.hpp"

struct pg_result;
typedef struct pg_result PGresult;

namespace el1::db::postgres
{
	using namespace io::text::string;
	using namespace io::collection::list;
	using namespace io::collection::map;
	using namespace system::task;

	struct TPostgresException;
	class TResultStream;
	class TStatement;
	class TPostgresConnection;

	static const u32_t N_FIFO_ROWS = 1024;

	struct oid_type_map_t
	{
		// ID of the type on the server (see `select * from pg_types`).
		// We assume these are constant - bad things will happen if they are not.
		unsigned int oid;

		// the size of the data type in postgres internal binary format
		// -1 for variable sized types with a size field
		// -2 for null-terminated strings of variable size
		s8_t sz_bytes;

		// name of the type in PostgreSQL
		const char* pg_name;

		// `type` specifies the C++ datatype produced by `deserialize()` / is expected by `serialize()`.
		const std::type_info* type;

		// First arg is a reference to this oid_type_map_t
		// Second arg is the input buffer.
		// Third arg the output buffer.
		// Forth arg is the number of bytes in the input buffer (only `deserialize()`).
		// `serialize()` returns the number of bytes stored (or would have been stored) in the output buffer.
		// `serialize()` allows the output buffer to be `nullptr`. In this case no data is written, but the required size of the output buffer is computed and returned. In this case the returned size may be bigger than what is actually required. However when passing an actual output buffer the returned size will always be accurate.
		// For `deserialize()` the output buffer must be large enough and correctly aligned to hold `type`.
		// If nullptr is passed as output buffer to `deserialize()` then only error checks are performed.
		util::function::TFunction<usys_t, const oid_type_map_t&, const void* const, void* const> serialize;
		util::function::TFunction<void,   const oid_type_map_t&, const void* const, void* const, const usys_t> deserialize;
		util::function::TFunction<void, void* const> destruct;
	};

	extern const oid_type_map_t ARR_OID_TYPE_MAP[];
	extern const usys_t N_OID_TYPE_MAP;
	static const usys_t SZ_FIELD_BUFFER = sizeof(void*) * 3;

	struct TPostgresException : IException
	{
		const TString message;

		TString Message() const final override;
		IException* Clone() const override;

		TPostgresException(void* pg_connection, const TString& description);
		TPostgresException(PGresult* pg_result, const TString& description);
	};

	struct TPostgresColumnDescription : TColumnDescription
	{
		u16_t idx_typemap;
		bool is_null;
		alignas(TString) byte_t buffer[SZ_FIELD_BUFFER];

		const oid_type_map_t& TypeInfo() const { return ARR_OID_TYPE_MAP[idx_typemap]; }
	};

	using result_ptr_t = std::unique_ptr<PGresult, void(*)(PGresult*)>;

	class TResultStream : public IResultStream
	{
		friend class TPostgresConnection;
		protected:
			TPostgresConnection* conn;
			io::stream::fifo::TFifo<void*, N_FIFO_ROWS> fifo;
			io::collection::list::TList<TPostgresColumnDescription> columns;
			const TString sql;

			TResultStream(TResultStream&&) = delete;
			TResultStream(const TResultStream&) = delete;
			TResultStream(TPostgresConnection* const conn, TString sql);

			result_ptr_t InternalMoveNext();
			void DeserializeResult(result_ptr_t current_result);

			void MoveFirst();

		public:
			TString SQL() const final override EL_GETTER;
			const system::waitable::IWaitable* OnNewData() const final override;
			bool IsMetadataReady() const final override;
			bool IsFirstRowReady() const final override;
			bool IsNextRowReady() const final override;

			usys_t CountColumns() const final override EL_GETTER;
			IDatabaseConnection* Connection() const final override EL_GETTER;
			bool End() const final override EL_GETTER;
			const TPostgresColumnDescription& Column(const usys_t index) const final override EL_GETTER;
			const void* Cell(const usys_t index) const final override EL_GETTER;
			void DiscardAllRows() final override;
			void MoveNext() final override;

			~TResultStream();
	};

	class TStatement : public IStatement
	{
		public:
			TString SQL() const final override EL_GETTER;
			usys_t CountArgs() const final override ;
			std::unique_ptr<IResultStream> Execute(array_t<query_arg_t> args) final override ;
	};

	class TChannelListener;

	struct TNotifyChannel
	{
		TPostgresConnection* conn;
		io::collection::list::TList<TChannelListener*> listeners;

		TNotifyChannel(TPostgresConnection* conn) : conn(conn) {}
	};

	class TChannelListener
	{
		friend class TPostgresConnection;
		protected:
			std::shared_ptr<TNotifyChannel> channel;
			io::collection::list::TList<std::shared_ptr<const TString>> pending_events;
			mutable system::waitable::TMemoryWaitable<usys_t> on_pending_event;

		public:
			TString Name() const EL_GETTER;	// will return empty string if the connection was closed
			system::waitable::IWaitable* OnNotify() const EL_GETTER;
			std::shared_ptr<const TString> Read();	// returns a pointer to the payload string, returns nullptr when no more events are currently pending - check `OnNotify()` to wait for further events

			TChannelListener(const TChannelListener&) = delete;
			TChannelListener(TChannelListener&&) = delete;
			TChannelListener(std::shared_ptr<TNotifyChannel> channel);
			~TChannelListener();
	};

	class TPostgresConnection : public IDatabaseConnection
	{
		friend class TResultStream;
		friend class TChannelListener;
		protected:
			void* pg_connection;

			io::collection::list::TList<TResultStream*> active_queries;
			io::collection::map::TSortedMap<TString, std::shared_ptr<TNotifyChannel> > notify_channels;

			TFiber fib_flusher;
			TFiber fib_reader;

			void FlusherMain();
			void ReaderMain();

			void StartNotifyChannel(const TString& channel_name);
			void ShutdownNotifyChannel(const TString& channel_name);

		public:
			TPostgresConnection();
			TPostgresConnection(const TSortedMap<TString, const TString>& properties);
			TPostgresConnection(const TPostgresConnection&) = delete;
			TPostgresConnection(TPostgresConnection&&) = delete;
			~TPostgresConnection();

			void Connect(const TSortedMap<TString, const TString>& properties);
			void Disconnect();

			std::unique_ptr<IStatement> Prepare(const TString& sql) final override;
			std::unique_ptr<IResultStream> Execute(const TString& sql, array_t<query_arg_t> args = array_t<query_arg_t>()) final override;

			std::unique_ptr<TChannelListener> SubscribeNotifyChannel(const TString& channel_name);
	};
}
#endif
