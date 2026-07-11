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
#include <cstddef>
#include <typeinfo>

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
	using oid_t = u32_t;

	struct IDatatypeCodec
	{
		const char* const namespace_name;
		const char* const datatype_name;
		const std::type_info* const cxx_type_info;
		const usys_t cxx_size;
		const usys_t cxx_alignment;

		IDatatypeCodec(const char* namespace_name, const char* datatype_name, const std::type_info& cxx_type_info, usys_t cxx_size, usys_t cxx_alignment);
		virtual ~IDatatypeCodec() = default;

		// If pg_out has no storage, return an upper bound for the encoded size.
		// Otherwise encode the value and return the exact number of bytes used.
		virtual usys_t Serialize(const void* const cxx_in, array_t<byte_t> pg_out) const = 0;
		virtual void Deserialize(array_t<const byte_t> pg_in, void* const cxx_out) const = 0;
		virtual void Destruct(const void* const cxx_in) const = 0;
	};

	class TTypeMap : public io::collection::map::TSortedMap<oid_t, const IDatatypeCodec*>
	{
		public:
			using TBase = io::collection::map::TSortedMap<oid_t, const IDatatypeCodec*>;
			using TCodecRegistry = io::collection::list::TList<const IDatatypeCodec*>;

			struct TCodecBinding
			{
				oid_t oid;
				const IDatatypeCodec* codec;
			};

			static TCodecRegistry GLOBAL_CODEC_REGISTRY;
			static TTypeMap LoadTypeMap(TPostgresConnection& conn, const TCodecRegistry& registry = GLOBAL_CODEC_REGISTRY);

			const IDatatypeCodec& LookupByOid(oid_t oid) const;
			const TCodecBinding& LookupByCxxType(const std::type_info& cxx_type_info) const;
			void Clear();

		private:
			io::collection::list::TList<TCodecBinding> cxx_type_map;
	};

	static const usys_t SZ_FIELD_BUFFER = sizeof(void*) * 8;
	static const usys_t ALIGN_FIELD_BUFFER = alignof(std::max_align_t);

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
		const IDatatypeCodec* codec;
		bool is_null;
		alignas(std::max_align_t) byte_t buffer[SZ_FIELD_BUFFER];

		TPostgresColumnDescription();
		~TPostgresColumnDescription();
	};

	using result_ptr_t = std::unique_ptr<PGresult, void(*)(PGresult*)>;

	class TResultStream : public IResultStream
	{
		friend class TPostgresConnection;
		protected:
			TPostgresConnection* conn;
			const TTypeMap type_map;
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
		protected:
			TPostgresConnection* const conn;
			const TString name;
			const TString sql;

		public:
			TString SQL() const final override EL_GETTER;
			std::unique_ptr<IResultStream> Execute(array_t<query_arg_t> args) final override;
			using IStatement::Execute;

			TStatement(TPostgresConnection* const conn, TString sql);
			~TStatement();
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
			system::waitable::TMemoryWaitable<usys_t>* OnNotify() const EL_GETTER;
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
		friend class TTypeMap;
		protected:
			void* pg_connection;
			TTypeMap type_map;

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
			using IDatabaseConnection::Execute;

			std::unique_ptr<TChannelListener> SubscribeNotifyChannel(const TString& channel_name);
	};
}
#endif
