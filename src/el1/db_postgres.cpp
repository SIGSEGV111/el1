#ifdef EL1_WITH_POSTGRES
#include "db_postgres.hpp"
#include "io_collection_list.hpp"
#include "error.hpp"
#include <libpq-fe.h>
#include <string.h>
#include <arpa/inet.h>

namespace el1::db::postgres
{
	using namespace io::collection::list;
	using namespace io::collection::map;

	bool type_key_t::operator!=(const type_key_t& rhs) const
	{
		return !((*this) == rhs);
	}

	bool type_key_t::operator==(const type_key_t& rhs) const
	{
		return memcmp(this, &rhs, sizeof(type_key_t)) == 0;
	}

	bool type_key_t::operator> (const type_key_t& rhs) const
	{
		return memcmp(this, &rhs, sizeof(type_key_t)) > 0;
	}

	bool type_key_t::operator< (const type_key_t& rhs) const
	{
		return memcmp(this, &rhs, sizeof(type_key_t)) < 0;
	}

	TSortedMap<type_key_t, type_convert_t> type_map;

	TString TPostgresException::Message() const
	{
		return this->message;
	}

	IException* TPostgresException::Clone() const
	{
		return new TPostgresException(*this);
	}

	#define PQ_ERROR(cmd, description) do { EL_ERROR( (cmd) != 1, TPostgresException, this->pg_connection, description); } while(false)

	TPostgresException::TPostgresException(void* pg_connection, const TString& description) : message(TString::Format("%s: %s (backend PID: %d)", description, PQerrorMessage((PGconn*)pg_connection), PQbackendPID((PGconn*)pg_connection)))
	{
	}

	TConnection::TConnection(const TSortedMap<TString, const TString>& properties) :
		on_rx_ready({ .read = true, .write = false, .other = false }),
		on_tx_ready({ .read = false, .write = true, .other = false }),
		pg_connection(nullptr),
		pg_result(nullptr),
		n_rows_cached(0),
		idx_row(-1),
		fiber_flusher(TFunction(this, &TConnection::FlusherMain), false)
	{
		TList<std::unique_ptr<char[]>> keys;
		TList<std::unique_ptr<char[]>> values;

		for(const auto& item : properties.Items())
		{
			keys.MoveAppend(item.key.MakeCStr());
			values.MoveAppend(item.value.MakeCStr());
		}

		keys.MoveAppend(std::unique_ptr<char[]>(nullptr));
		values.MoveAppend(std::unique_ptr<char[]>(nullptr));

		EL_ERROR((this->pg_connection = PQconnectdbParams((const char* const*)&keys[0], (const char* const*)&values[0], 1)) == nullptr, TException, "unable to create postgres connection object");

		EL_ERROR(PQstatus((PGconn*)this->pg_connection) != CONNECTION_OK, TPostgresException, this->pg_connection, "connection to database failed");

		EL_ERROR(PQsetnonblocking((PGconn*)this->pg_connection, 1) != 0, TPostgresException, this->pg_connection, "unable to set non-blocking mode");
		PQ_ERROR(PQenterPipelineMode((PGconn*)this->pg_connection), "unable to activate pipeline mode");
		PQflush((PGconn*)this->pg_connection);

		const fd_t fd = PQsocket((PGconn*)this->pg_connection);
		on_rx_ready.Handle(fd);
		on_tx_ready.Handle(fd);

		fiber_flusher.Start();
	}

	TConnection::~TConnection()
	{
		fiber_flusher.Shutdown();
		auto e = fiber_flusher.Join();

		if(e != nullptr)
		{
			e->Print("TConnection::~TConnection()");
		}

		if(this->pg_result != nullptr)
			PQclear((PGresult*)this->pg_result);

		if(this->pg_connection != nullptr)
			PQfinish((PGconn*)this->pg_connection);
	}

	void TConnection::_Prepare(const TString& name, const TString& sql, const array_t<oid_t> param_oids)
	{
		PQ_ERROR(PQsendPrepare((PGconn*)this->pg_connection, name.MakeCStr().get(), sql.MakeCStr().get(), param_oids.Count(), &param_oids[0]), "unable to send prepare command");
	}

	void TConnection::_Execute(const TString& sql, const array_t<const data_t> args)
	{
		if(args.Count() > 0)
		{
			TList<oid_t> types = args.Pipe().Map([](const data_t& data){ return data.oid; }).Collect();
			TList<const char*> values = args.Pipe().Map([](const data_t& data){ return (const char*)data.data; }).Collect();
			TList<int> lengths = args.Pipe().Map(
				[](const data_t& data)
				{
					EL_ERROR(data.length <= 0, TException, TString::Format("invalid data length (oid = %u, length = %d, data = %p)", data.oid, data.length, (usys_t)data.data));
					return (int)data.length;
				}
			).Collect();

			TList<int> formats = args.Pipe().Map([](auto&){ return 1; }).Collect();

			PQ_ERROR(PQsendQueryParams((PGconn*)this->pg_connection, sql.MakeCStr().get(), args.Count(), &types[0], &values[0], &lengths[0], &formats[0], 1), "failed to send async query");
		}
		else
		{
			PQ_ERROR(PQsendQueryParams((PGconn*)this->pg_connection, sql.MakeCStr().get(), args.Count(), nullptr, nullptr, nullptr, nullptr, 1), "failed to send async query");
		}

		PQsetSingleRowMode((PGconn*)this->pg_connection); // may return 0 if the statement failed
		PQ_ERROR(PQsendFlushRequest((PGconn*)this->pg_connection), "failed to request buffer flush");

		const int r = PQflush((PGconn*)this->pg_connection);
		EL_ERROR(r == -1, TPostgresException, this->pg_connection, "unable to flush output buffer to server");
		if(r != 0)
			fiber_flusher.Resume();
	}

	unsigned TConnection::CountRows() const
	{
		EL_ERROR(this->pg_result == nullptr, TException, "no result active on this connection");
		return PQntuples((PGresult*)this->pg_result);
	}

	unsigned TConnection::CountColumns() const
	{
		EL_ERROR(this->pg_result == nullptr, TException, "no result active on this connection");
		return PQnfields((PGresult*)this->pg_result);
	}

	unsigned TConnection::ColumnNameToIndex(const TString& name) const
	{
		EL_ERROR(this->pg_result == nullptr, TException, "no result active on this connection");
		const int r = PQfnumber((PGresult*)this->pg_result, name.MakeCStr().get());
		EL_ERROR(r < 0, TException, TString::Format("no such column %q", name));
		return r;
	}

	void TConnection::FlusherMain()
	{
		// will be terminated by shutdown_t exception
		for(;;)
		{
			const int r = PQflush((PGconn*)this->pg_connection);
			EL_ERROR(r < 0, TPostgresException, this->pg_connection, "unable to flush output buffer to server");
			if(r > 0)
				on_tx_ready.WaitFor();
			else // r == 0
				TFiber::Self()->Stop();
		}
	}

	bool TConnection::FetchNextRow()
	{
		if(n_rows_cached > 0)
		{
			idx_row++;
			n_rows_cached--;
			return true;
		}

		if(this->pg_result != nullptr)
			PQclear((PGresult*)this->pg_result);

		// this code block is required to ensure el1 can handle blocking of this fiber and consequently call the scheduler
		PQ_ERROR(PQconsumeInput((PGconn*)this->pg_connection), "failed consume input data");
		while(PQisBusy((PGconn*)this->pg_connection) == 1)
		{
			on_rx_ready.WaitFor();
			PQ_ERROR(PQconsumeInput((PGconn*)this->pg_connection), "failed consume input data");
		}

		// PQgetResult() will block if it lacks data from the server
		this->pg_result = PQgetResult((PGconn*)this->pg_connection);

		if(this->pg_result != nullptr)
		{
			idx_row++;

			const ExecStatusType status = PQresultStatus((PGresult*)this->pg_result);
			switch(status)
			{
				case PGRES_EMPTY_QUERY: // stupid but not an error?
				case PGRES_COMMAND_OK:
				case PGRES_TUPLES_OK:
				case PGRES_COPY_OUT:
				case PGRES_COPY_IN:
				case PGRES_COPY_BOTH:
				case PGRES_SINGLE_TUPLE:
				case PGRES_PIPELINE_SYNC:
					// all good
					break;

				case PGRES_BAD_RESPONSE:
				case PGRES_NONFATAL_ERROR:
				case PGRES_FATAL_ERROR:
				case PGRES_PIPELINE_ABORTED:
					PQpipelineSync((PGconn*)this->pg_connection);
					EL_THROW(TPostgresException, this->pg_connection, TString::Format("server returned error status: %s: %s", PQresStatus(status), PQresultErrorMessage((PGresult*)this->pg_result)));

				default:
					PQpipelineSync((PGconn*)this->pg_connection);
					EL_THROW(TPostgresException, this->pg_connection, "unknown result status");
			}

			const unsigned n_columns = CountColumns();
			const unsigned n_rows = CountRows();
			n_rows_cached =  n_rows - 1;

			for(unsigned j = 0; j < n_rows; j++)
				for(unsigned i = 0; i < n_columns; i++)
				{
					const int oid = PQftype((PGresult*)this->pg_result, i);

					if(PQgetisnull((PGresult*)this->pg_result, j, i) == 0)
					{
						void* data = PQgetvalue((PGresult*)this->pg_result, j, i);

						if(oid == 21) // 16bit
						{
							*(uint16_t*)data = be16toh(*(uint16_t*)data);
						}
						else if(oid == 23 || oid == 700) // 32bit
						{
							*(uint32_t*)data = be32toh(*(uint32_t*)data);
						}
						else if(oid == 20 || oid == 701) // 64bit
						{
							*(uint64_t*)data = be64toh(*(uint64_t*)data);
						}
					}
				}
		}
		else
			idx_row = -1;

		return this->pg_result != nullptr;
	}

	void TConnection::_ReadCellData(array_t<data_t> cells)
	{
		const unsigned n_cells = (unsigned)cells.Count();
		EL_ERROR(n_cells != CountColumns(), TInvalidArgumentException, "cells", "number of columns in 'cells' does not match resultset");

		for(unsigned i = 0; i < n_cells; i++)
		{
			data_t& cell = cells[i];
			cell.oid = PQftype((PGresult*)this->pg_result, i);

			if(PQgetisnull((PGresult*)this->pg_result, idx_row, i) == 0)
			{
				cell.length = PQgetlength((PGresult*)this->pg_result, idx_row, i);
				cell.data = PQgetvalue((PGresult*)this->pg_result, idx_row, i);
			}
			else
			{
				cell.length = 0;
				cell.data = nullptr;
			}
		}
	}

	void TConnection::DiscardResults()
	{
		while(FetchNextRow());

		// if(this->pg_result != nullptr)
		// {
		// 	PQclear((PGresult*)this->pg_result);
		// 	this->pg_result = nullptr;
		// }
  //
		// for(;;)
		// {
		// 	// this code block is required to ensure el1 can handle blocking of this fiber and consequently call the scheduler
		// 	PQ_ERROR(PQconsumeInput((PGconn*)this->pg_connection), "failed consume input data");
		// 	while(PQisBusy((PGconn*)this->pg_connection) == 1)
		// 	{
		// 		on_rx_ready.WaitFor();
		// 		PQ_ERROR(PQconsumeInput((PGconn*)this->pg_connection), "failed consume input data");
		// 	}
  //
		// 	// two times nullptr is required to truely ensure all resultsets have been discarded
		// 	PGresult* result = PQgetResult((PGconn*)this->pg_connection);
		// 	if(result == nullptr)
		// 	{
		// 		result = PQgetResult((PGconn*)this->pg_connection);
		// 		if(result == nullptr)
		// 			break;
		// 	}
  //
		// 	PQclear(result);
		// }
  //
		// n_rows_cached = 0;
		// idx_row = -1;
	}
}
#endif
