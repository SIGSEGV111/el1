#pragma once

#include "io_stream.hpp"
#include "io_text_string.hpp"

namespace el1::db
{
	using namespace io::types;
	using namespace io::text::string;
	using namespace io::collection::list;

	struct TColumnDescription;
	class IResultStream;
	struct IDatabaseConnection;

	struct query_arg_t
	{
		const std::type_info* type;	// can be NULL *only* when `value` is also NULL
		const void* value;	// can be NULL
		s32_t sz_bytes;	// size of value in bytes
	};

	struct TColumnDescription
	{
		TString name;
		const std::type_info* type;
	};

	template<typename ... A>
	class TResultPipe;

	class IResultStream
	{
		private:
			template<typename T, std::size_t I>
			const T* _CheckAndCastCellValue(const void* const value) const;

			template<typename TTuple, std::size_t ... I>
			TTuple _RowToTuple(std::index_sequence<I...>) const;

		public:
			virtual ~IResultStream() {}

			// this function may return nullptr in case the connection was closed
			virtual IDatabaseConnection* Connection() const EL_GETTER = 0;

			virtual TString SQL() const EL_GETTER = 0;

			// returns true once the end of the stream has been reached
			virtual bool End() const EL_GETTER = 0;

			// signals when new data arrived
			// once EOF has been reached this function will return nullptr
			// check `IsMetadataReady()`, `IsFirstRowReady()` and `IsNextRowReady()` to see if anything changed
			virtual const system::waitable::IWaitable* OnNewData() const EL_GETTER = 0;

			// returns true once the column description (`Columns()`) and other required metadata was loaded
			virtual bool IsMetadataReady() const EL_GETTER = 0;

			// returns true once the first row was loaded and can be accessed with `Row()`
			// this function keeps returning true afterwards, despite not beeing at the first row
			virtual bool IsFirstRowReady() const EL_GETTER = 0;

			// returns true when `MoveNext()` can be called without blocking
			// that does not imply that there is actually a next row - check `End()` to see if a row is currently loaded or not
			virtual bool IsNextRowReady() const EL_GETTER = 0;

			// all below functions are blocking

			virtual usys_t CountColumns() const EL_GETTER = 0;
			virtual const TColumnDescription& Column(const usys_t index) const EL_GETTER = 0;
			virtual const void* Cell(const usys_t index) const EL_GETTER = 0;
			virtual void DiscardAllRows() = 0;	// skips over all rows to the end of the result stream to receive any pending errors that might have occured during processing and to unblock the connection for following result streams
			virtual void MoveNext() = 0;	// moves the stream to the next row

			template<typename ... A>
			std::tuple<const A* ...> Row() const;
	};

	template<typename ... A>
	class TResultPipe : public io::stream::IPipe<TResultPipe<A...>, std::tuple<const A* ...>> // TODO remove ISource<T> from IPipe<T>
	{
		public:
			using TOut = std::tuple<A...>;
			TResultPipe(IResultStream* const rs);
			const TOut* NextItem() final override;

		protected:
			IResultStream* const rs;
			TOut current_row;
	};

	struct IStatement
	{
		virtual ~IStatement() {}
		virtual TString SQL() const EL_GETTER = 0;
		virtual usys_t CountArgs() const = 0;
		virtual std::unique_ptr<IResultStream> Execute(array_t<query_arg_t> args) = 0;
	};

	struct IDatabaseConnection
	{
		virtual ~IDatabaseConnection() {}
		virtual std::unique_ptr<IStatement> Prepare(const TString& sql) = 0;
		virtual std::unique_ptr<IResultStream> Execute(const TString& sql, array_t<query_arg_t> args = array_t<query_arg_t>()) = 0;

		std::unique_ptr<IResultStream> Execute(const TString& sql, const TList<query_arg_t>& args) { return Execute(sql, (array_t<query_arg_t>)args); }

		template<typename ... A>
		std::unique_ptr<IResultStream> Execute(const TString& sql, const A& ... a);
	};

	/***************************************************************************************************/
	/***************************************************************************************************/
	/***************************************************************************************************/

	template<typename T, std::size_t I>
	const T* IResultStream::_CheckAndCastCellValue(const void* const value) const
	{
		EL_ERROR( !(*Column(I).type == typeid(T) || typeid(T) == typeid(void)), TInvalidArgumentException, "T", "wrong datatype for result field");
		return reinterpret_cast<const T*>(value);
	}

	template<typename TTuple, std::size_t ... I>
	TTuple IResultStream::_RowToTuple(std::index_sequence<I...>) const
	{
		return TTuple( _CheckAndCastCellValue<std::remove_const_t<std::remove_pointer_t<std::tuple_element_t<I, TTuple>>>, I>(Cell(I)) ... );
	}

	template<typename ... A>
	std::tuple<const A* ...> IResultStream::Row() const
	{
		try
		{
			const auto n_columns = CountColumns();
			EL_ERROR(End(), TException, "EOF reached");
			EL_ERROR(n_columns == 0, TException, "query did not produce any output");
			EL_ERROR(sizeof...(A) != n_columns, TInvalidArgumentException, "A...", "the number of template arguments does not match with the number of columns in the query result");
			return _RowToTuple<std::tuple<const A* ...> >(std::make_index_sequence<sizeof...(A)>());
		}
		catch(const error::IException& e)
		{
			EL_FORWARD(e, TException, TString::Format("while processing result of query %q", SQL()));
		}
	}

	template<typename ... A>
	const TResultPipe<A ...>::TOut* TResultPipe<A ...>::NextItem()
	{
		rs->MoveNext();

		if(rs->End())
		{
			current_row = rs->Row<A...>();
			return &current_row;
		}
		else
		{
			return nullptr;
		}
	}

	template<typename ... A>
	TResultPipe<A ...>::TResultPipe(IResultStream* const rs) : rs(rs)
	{
	}

	template<typename T>
	static void _AddQueryArg(TList<query_arg_t>& args, const T& v)
	{
		args.Append({
			.type = &typeid(T),
			.value = &v,
			.sz_bytes = sizeof(T)
		});
	}

	template<typename T, typename ... A>
	static void _AddQueryArg(TList<query_arg_t>& args, const T& v, const A& ... a)
	{
		_AddQueryArg(args, v);
		_AddQueryArg(args, a ...);
	}

	template<typename ... A>
	std::unique_ptr<IResultStream> IDatabaseConnection::Execute(const TString& sql, const A& ... a)
	{
		TList<query_arg_t> args;
		_AddQueryArg(args, a ...);
		return Execute(sql, (array_t<query_arg_t>)args);
	}
}
