#ifdef EL1_WITH_POSTGRES
#include "debug.hpp"
#include "db_postgres.hpp"
#include "io_collection_list.hpp"
#include "io_format_json.hpp"
#include "io_path.hpp"
#include "io_text_encoding_utf8.hpp"
#include "error.hpp"
#include <atomic>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <endian.h>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <libpq-fe.h>

namespace el1::db::postgres
{
	using namespace io::format::json;
	using namespace io::collection::list;

	IDatatypeCodec::IDatatypeCodec(const char* const namespace_name, const char* const datatype_name, const std::type_info& cxx_type_info, const usys_t cxx_size, const usys_t cxx_alignment, const bool required) :
		namespace_name(namespace_name),
		datatype_name(datatype_name),
		cxx_type_info(&cxx_type_info),
		cxx_size(cxx_size),
		cxx_alignment(cxx_alignment),
		required(required)
	{
	}

	static void ValidateOutputSize(array_t<byte_t> pg_out, const usys_t required_size)
	{
		EL_ERROR(pg_out.Count() < required_size, TException, TString::Format("datatype codec output buffer is too small (required: %d bytes, available: %d bytes)", (u64_t)required_size, (u64_t)pg_out.Count()));
	}

	template<typename T>
	class TTypedDatatypeCodec : public IDatatypeCodec
	{
		public:
			TTypedDatatypeCodec(const char* const namespace_name, const char* const datatype_name, const bool required = true) : IDatatypeCodec(namespace_name, datatype_name, typeid(T), sizeof(T), alignof(T), required)
			{
			}

			void Destruct(void* const cxx_in) const final override
			{
				if constexpr(!std::is_trivially_destructible_v<T>)
					reinterpret_cast<const T*>(cxx_in)->~T();
			}
	};

	class TNoOpDatatypeCodec final : public IDatatypeCodec
	{
		public:
			TNoOpDatatypeCodec(const char* const namespace_name, const char* const datatype_name, const bool required = true) : IDatatypeCodec(namespace_name, datatype_name, typeid(void), 0, 1, required)
			{
			}

			usys_t Serialize(const void* const, array_t<byte_t>) const final override
			{
				return 0;
			}

			void Deserialize(const array_t<const byte_t>, void* const) const final override
			{
			}

			void Destruct(void* const) const final override
			{
			}
	};

	class TBooleanDatatypeCodec final : public TTypedDatatypeCodec<bool>
	{
		public:
			using TTypedDatatypeCodec<bool>::TTypedDatatypeCodec;

			usys_t Serialize(const void* const cxx_in, array_t<byte_t> pg_out) const final override
			{
				if(pg_out.Count() > 0)
				{
					ValidateOutputSize(pg_out, 1);
					pg_out[0] = *reinterpret_cast<const bool*>(cxx_in) ? 1 : 0;
				}
				return 1;
			}

			void Deserialize(const array_t<const byte_t> pg_in, void* const cxx_out) const final override
			{
				EL_ERROR(pg_in.Count() != 1 || pg_in[0] > 1, TException, "invalid PostgreSQL bool binary value");
				new (cxx_out) bool(pg_in[0] != 0);
			}
	};

	class TCharDatatypeCodec final : public TTypedDatatypeCodec<char>
	{
		public:
			using TTypedDatatypeCodec<char>::TTypedDatatypeCodec;

			usys_t Serialize(const void* const cxx_in, array_t<byte_t> pg_out) const final override
			{
				if(pg_out.Count() > 0)
				{
					ValidateOutputSize(pg_out, 1);
					memcpy(pg_out.ItemPtr(0), cxx_in, 1);
				}
				return 1;
			}

			void Deserialize(const array_t<const byte_t> pg_in, void* const cxx_out) const final override
			{
				EL_ERROR(pg_in.Count() != 1, TException, "invalid PostgreSQL char binary value");
				new (cxx_out) char(reinterpret_cast<const char*>(pg_in.ItemPtr(0))[0]);
			}
	};

	template<typename T>
	class TIntegerDatatypeCodec final : public TTypedDatatypeCodec<T>
	{
		public:
			using TTypedDatatypeCodec<T>::TTypedDatatypeCodec;

			usys_t Serialize(const void* const cxx_in, array_t<byte_t> pg_out) const final override
			{
				if(pg_out.Count() > 0)
				{
					ValidateOutputSize(pg_out, sizeof(T));
					using TUnsigned = std::make_unsigned_t<T>;
					TUnsigned value;
					memcpy(&value, cxx_in, sizeof(value));

					if constexpr(sizeof(T) == 1)
					{
					}
					else if constexpr(sizeof(T) == 2)
						value = htobe16(value);
					else if constexpr(sizeof(T) == 4)
						value = htobe32(value);
					else if constexpr(sizeof(T) == 8)
						value = htobe64(value);
					else
						EL_THROW(TLogicException);

					memcpy(pg_out.ItemPtr(0), &value, sizeof(value));
				}
				return sizeof(T);
			}

			void Deserialize(const array_t<const byte_t> pg_in, void* const cxx_out) const final override
			{
				EL_ERROR(pg_in.Count() != sizeof(T), TException, TString::Format("invalid PostgreSQL integer binary size (expected: %d bytes, received: %d bytes)", (u64_t)sizeof(T), (u64_t)pg_in.Count()));
				using TUnsigned = std::make_unsigned_t<T>;
				TUnsigned value;
				memcpy(&value, pg_in.ItemPtr(0), sizeof(value));

				if constexpr(sizeof(T) == 1)
				{
				}
				else if constexpr(sizeof(T) == 2)
					value = be16toh(value);
				else if constexpr(sizeof(T) == 4)
					value = be32toh(value);
				else if constexpr(sizeof(T) == 8)
					value = be64toh(value);
				else
					EL_THROW(TLogicException);

				T decoded;
				memcpy(&decoded, &value, sizeof(decoded));
				new (cxx_out) T(decoded);
			}
	};

	template<typename T>
	class TFloatDatatypeCodec final : public TTypedDatatypeCodec<T>
	{
		public:
			using TTypedDatatypeCodec<T>::TTypedDatatypeCodec;

			usys_t Serialize(const void* const cxx_in, array_t<byte_t> pg_out) const final override
			{
				if(pg_out.Count() > 0)
				{
					ValidateOutputSize(pg_out, sizeof(T));
					if constexpr(sizeof(T) == 4)
					{
						u32_t value;
						memcpy(&value, cxx_in, sizeof(value));
						value = htobe32(value);
						memcpy(pg_out.ItemPtr(0), &value, sizeof(value));
					}
					else if constexpr(sizeof(T) == 8)
					{
						u64_t value;
						memcpy(&value, cxx_in, sizeof(value));
						value = htobe64(value);
						memcpy(pg_out.ItemPtr(0), &value, sizeof(value));
					}
					else
						EL_THROW(TLogicException);
				}
				return sizeof(T);
			}

			void Deserialize(const array_t<const byte_t> pg_in, void* const cxx_out) const final override
			{
				EL_ERROR(pg_in.Count() != sizeof(T), TException, TString::Format("invalid PostgreSQL floating-point binary size (expected: %d bytes, received: %d bytes)", (u64_t)sizeof(T), (u64_t)pg_in.Count()));
				T decoded;
				if constexpr(sizeof(T) == 4)
				{
					u32_t value;
					memcpy(&value, pg_in.ItemPtr(0), sizeof(value));
					value = be32toh(value);
					memcpy(&decoded, &value, sizeof(decoded));
				}
				else if constexpr(sizeof(T) == 8)
				{
					u64_t value;
					memcpy(&value, pg_in.ItemPtr(0), sizeof(value));
					value = be64toh(value);
					memcpy(&decoded, &value, sizeof(decoded));
				}
				else
					EL_THROW(TLogicException);

				new (cxx_out) T(decoded);
			}
	};

	class TByteArrayDatatypeCodec final : public TTypedDatatypeCodec<TList<byte_t>>
	{
		public:
			using TTypedDatatypeCodec<TList<byte_t>>::TTypedDatatypeCodec;

			usys_t Serialize(const void* const cxx_in, array_t<byte_t> pg_out) const final override
			{
				const TList<byte_t>& value = *reinterpret_cast<const TList<byte_t>*>(cxx_in);
				if(pg_out.Count() > 0)
				{
					ValidateOutputSize(pg_out, value.Count());
					if(value.Count() > 0)
						memcpy(pg_out.ItemPtr(0), value.ItemPtr(0), value.Count());
				}
				return value.Count();
			}

			void Deserialize(const array_t<const byte_t> pg_in, void* const cxx_out) const final override
			{
				new (cxx_out) TList<byte_t>(pg_in);
			}
	};

	class TStringDatatypeCodec final : public TTypedDatatypeCodec<TString>
	{
		public:
			using TTypedDatatypeCodec<TString>::TTypedDatatypeCodec;

			usys_t Serialize(const void* const cxx_in, array_t<byte_t> pg_out) const final override
			{
				const TString& value = *reinterpret_cast<const TString*>(cxx_in);
				EL_ERROR(value.Length() > std::numeric_limits<usys_t>::max() / 4U, TException, "string is too large to encode as UTF-8");
				const usys_t estimated_size = value.Length() * 4U;
				if(pg_out.Count() == 0)
					return estimated_size;

				ValidateOutputSize(pg_out, estimated_size);
				return value.chars.Pipe().Transform(io::text::encoding::utf8::TUTF8Encoder()).Read(pg_out.ItemPtr(0), pg_out.Count());
			}

			void Deserialize(const array_t<const byte_t> pg_in, void* const cxx_out) const final override
			{
				new (cxx_out) TString(reinterpret_cast<const char*>(pg_in.ItemPtr(0)), pg_in.Count());
			}
	};

	class TTimestampDatatypeCodec final : public TTypedDatatypeCodec<TTime>
	{
		public:
			using TTypedDatatypeCodec<TTime>::TTypedDatatypeCodec;

			usys_t Serialize(const void* const cxx_in, array_t<byte_t> pg_out) const final override
			{
				if(pg_out.Count() > 0)
				{
					ValidateOutputSize(pg_out, 8);
					const TTime& timestamp = *reinterpret_cast<const TTime*>(cxx_in);
					const s64_t microseconds = timestamp.ConvertToI(EUnit::MICROSECONDS) - 946684800000000LL;
					u64_t encoded;
					memcpy(&encoded, &microseconds, sizeof(encoded));
					encoded = htobe64(encoded);
					memcpy(pg_out.ItemPtr(0), &encoded, sizeof(encoded));
				}
				return 8;
			}

			void Deserialize(const array_t<const byte_t> pg_in, void* const cxx_out) const final override
			{
				EL_ERROR(pg_in.Count() != 8, TException, "invalid PostgreSQL timestamp binary value");
				u64_t encoded;
				memcpy(&encoded, pg_in.ItemPtr(0), sizeof(encoded));
				encoded = be64toh(encoded);
				s64_t microseconds;
				memcpy(&microseconds, &encoded, sizeof(microseconds));
				new (cxx_out) TTime(TTime::ConvertFrom(EUnit::MICROSECONDS, microseconds + 946684800000000LL));
			}
	};

	class TJsonDatatypeCodec final : public TTypedDatatypeCodec<TJsonValue>
	{
		protected:
			const bool is_jsonb;

		public:
			TJsonDatatypeCodec(const char* const namespace_name, const char* const datatype_name, const bool is_jsonb) : TTypedDatatypeCodec<TJsonValue>(namespace_name, datatype_name), is_jsonb(is_jsonb)
			{
			}

			usys_t Serialize(const void* const cxx_in, array_t<byte_t> pg_out) const final override
			{
				const TJsonValue& value = *reinterpret_cast<const TJsonValue*>(cxx_in);
				const TString text = value.ToString();
				const usys_t header_size = is_jsonb ? 1U : 0U;
				EL_ERROR(text.Length() > (std::numeric_limits<usys_t>::max() - header_size) / 4U, TException, "JSON value is too large to encode as UTF-8");
				const usys_t estimated_size = header_size + text.Length() * 4U;
				if(pg_out.Count() == 0)
					return estimated_size;

				ValidateOutputSize(pg_out, estimated_size);
				if(is_jsonb)
					pg_out[0] = 1;

				array_t<byte_t> payload(pg_out.ItemPtr(0) + header_size, pg_out.Count() - header_size);
				const usys_t payload_size = text.chars.Pipe().Transform(io::text::encoding::utf8::TUTF8Encoder()).Read(payload.ItemPtr(0), payload.Count());
				return header_size + payload_size;
			}

			void Deserialize(const array_t<const byte_t> pg_in, void* const cxx_out) const final override
			{
				usys_t offset = 0;
				if(is_jsonb)
				{
					EL_ERROR(pg_in.Count() == 0 || pg_in[0] != 1, TException, "unsupported PostgreSQL jsonb binary version");
					offset = 1;
				}

				const TString text(reinterpret_cast<const char*>(pg_in.ItemPtr(offset)), pg_in.Count() - offset);
				new (cxx_out) TJsonValue(TJsonValue::Parse(text));
			}
	};

	class TLTreeDatatypeCodec final : public TTypedDatatypeCodec<io::path::TPath>
	{
		public:
			TLTreeDatatypeCodec(const char* const namespace_name, const char* const datatype_name, const bool required) : TTypedDatatypeCodec<io::path::TPath>(namespace_name, datatype_name, required)
			{
			}

			usys_t Serialize(const void* const cxx_in, array_t<byte_t> pg_out) const final override
			{
				const io::path::TPath& value = *reinterpret_cast<const io::path::TPath*>(cxx_in);
				EL_ERROR(value.Separator() != TUTF32('.'), TInvalidArgumentException, "cxx_in", "ltree paths must use '.' as their component separator");

				const TString text = value.ToString();
				EL_ERROR(text.Length() > (std::numeric_limits<usys_t>::max() - 1U) / 4U, TException, "ltree value is too large to encode as UTF-8");
				const usys_t estimated_size = 1U + text.Length() * 4U;
				if(pg_out.Count() == 0)
					return estimated_size;

				ValidateOutputSize(pg_out, estimated_size);
				pg_out[0] = 1;
				array_t<byte_t> payload(pg_out.ItemPtr(1), pg_out.Count() - 1U);
				const usys_t payload_size = text.chars.Pipe().Transform(io::text::encoding::utf8::TUTF8Encoder()).Read(payload.ItemPtr(0), payload.Count());
				return 1U + payload_size;
			}

			void Deserialize(const array_t<const byte_t> pg_in, void* const cxx_out) const final override
			{
				EL_ERROR(pg_in.Count() == 0 || pg_in[0] != 1, TException, "unsupported PostgreSQL ltree binary version");
				const TString text(reinterpret_cast<const char*>(pg_in.ItemPtr(1)), pg_in.Count() - 1U);
				new (cxx_out) io::path::TPath(text, '.');
			}
	};

	static const TBooleanDatatypeCodec CODEC_BOOL("pg_catalog", "bool");
	static const TByteArrayDatatypeCodec CODEC_BYTEA("pg_catalog", "bytea");
	static const TCharDatatypeCodec CODEC_CHAR("pg_catalog", "char");
	static const TIntegerDatatypeCodec<s64_t> CODEC_INT8("pg_catalog", "int8");
	static const TIntegerDatatypeCodec<s16_t> CODEC_INT2("pg_catalog", "int2");
	static const TIntegerDatatypeCodec<s32_t> CODEC_INT4("pg_catalog", "int4");
	static const TStringDatatypeCodec CODEC_TEXT("pg_catalog", "text");
	static const TIntegerDatatypeCodec<s32_t> CODEC_OID("pg_catalog", "oid");
	static const TJsonDatatypeCodec CODEC_JSON("pg_catalog", "json", false);
	static const TStringDatatypeCodec CODEC_XML("pg_catalog", "xml");
	static const TFloatDatatypeCodec<float> CODEC_FLOAT4("pg_catalog", "float4");
	static const TFloatDatatypeCodec<double> CODEC_FLOAT8("pg_catalog", "float8");
	static const TNoOpDatatypeCodec CODEC_UNKNOWN("pg_catalog", "unknown");
	static const TStringDatatypeCodec CODEC_VARCHAR("pg_catalog", "varchar");
	static const TTimestampDatatypeCodec CODEC_TIMESTAMP("pg_catalog", "timestamp");
	static const TTimestampDatatypeCodec CODEC_TIMESTAMPTZ("pg_catalog", "timestamptz");
	static const TJsonDatatypeCodec CODEC_JSONB("pg_catalog", "jsonb", true);
	static const TStringDatatypeCodec CODEC_CSTRING("pg_catalog", "cstring");
	static const TLTreeDatatypeCodec CODEC_LTREE("*", "ltree", false);

	TTypeMap::TCodecRegistry TTypeMap::GLOBAL_CODEC_REGISTRY = {
		&CODEC_BOOL,
		&CODEC_BYTEA,
		&CODEC_CHAR,
		&CODEC_INT8,
		&CODEC_INT2,
		&CODEC_INT4,
		&CODEC_TEXT,
		&CODEC_OID,
		&CODEC_JSON,
		&CODEC_XML,
		&CODEC_FLOAT4,
		&CODEC_FLOAT8,
		&CODEC_UNKNOWN,
		&CODEC_VARCHAR,
		&CODEC_TIMESTAMP,
		&CODEC_TIMESTAMPTZ,
		&CODEC_JSONB,
		&CODEC_CSTRING,
		&CODEC_LTREE,
	};

	void TTypeMap::Clear()
	{
		TBase::Clear();
		cxx_type_map.Clear();
	}

	const IDatatypeCodec& TTypeMap::LookupByOid(const oid_t oid) const
	{
		const IDatatypeCodec* const* const codec = TBase::Get(oid);
		EL_ERROR(codec == nullptr, TException, TString::Format("PostgreSQL datatype OID %d has no registered codec", oid));
		return **codec;
	}

	const TTypeMap::TCodecBinding& TTypeMap::LookupByCxxType(const std::type_info& cxx_type_info) const
	{
		for(const TCodecBinding& binding : cxx_type_map)
			if(*binding.codec->cxx_type_info == cxx_type_info)
				return binding;

		EL_THROW(TException, TString::Format("C++ datatype %q has no registered PostgreSQL codec", debug::Demangle(cxx_type_info.name())));
	}


	static oid_t ParseOid(const char* const oid_text)
	{
		char* oid_end = nullptr;
		errno = 0;
		const unsigned long parsed_oid = strtoul(oid_text, &oid_end, 10);
		EL_ERROR(errno != 0 || oid_end == oid_text || *oid_end != 0 || parsed_oid > std::numeric_limits<oid_t>::max(), TLogicException);
		return (oid_t)parsed_oid;
	}

	TTypeMap TTypeMap::LoadTypeMap(TPostgresConnection& conn, const TCodecRegistry& registry)
	{
		EL_ERROR(conn.pg_connection == nullptr, TException, "cannot load PostgreSQL datatype map without an active connection");

		for(usys_t i = 0; i < registry.Count(); i++)
		{
			const IDatatypeCodec* const codec = registry[i];
			EL_ERROR(codec == nullptr, TException, "PostgreSQL datatype codec registry contains a NULL entry");
			EL_ERROR(codec->namespace_name == nullptr || codec->datatype_name == nullptr || codec->cxx_type_info == nullptr, TException, "PostgreSQL datatype codec has incomplete metadata");
			EL_ERROR(codec->cxx_alignment == 0, TException, "PostgreSQL datatype codec has invalid C++ alignment metadata");

			for(usys_t j = 0; j < i; j++)
			{
				const IDatatypeCodec* const previous = registry[j];
				EL_ERROR(strcmp(codec->namespace_name, previous->namespace_name) == 0 && strcmp(codec->datatype_name, previous->datatype_name) == 0, TException, TString::Format("PostgreSQL datatype codec %s.%s is registered more than once", codec->namespace_name, codec->datatype_name));
			}
		}

		result_ptr_t result(PQexec((PGconn*)conn.pg_connection,
			"SELECT t.oid, n.nspname, t.typname, t.typtype, t.typbasetype "
			"FROM pg_catalog.pg_type AS t "
			"JOIN pg_catalog.pg_namespace AS n ON n.oid = t.typnamespace"), &PQclear);
		EL_ERROR(result == nullptr, TPostgresException, conn.pg_connection, "unable to query PostgreSQL datatype catalog");
		EL_ERROR(PQresultStatus(result.get()) != PGRES_TUPLES_OK, TPostgresException, result.get(), "unable to query PostgreSQL datatype catalog");

		TList<oid_t> resolved_oids;
		resolved_oids.SetCount(registry.Count());
		for(oid_t& oid : resolved_oids)
			oid = 0;

		struct TDomainType
		{
			oid_t oid;
			oid_t base_oid;
		};
		TList<TDomainType> domain_types;

		const int n_rows = PQntuples(result.get());
		for(int row = 0; row < n_rows; row++)
		{
			const char* const oid_text = PQgetvalue(result.get(), row, 0);
			const char* const namespace_name = PQgetvalue(result.get(), row, 1);
			const char* const datatype_name = PQgetvalue(result.get(), row, 2);
			const char type_class = PQgetvalue(result.get(), row, 3)[0];
			const oid_t oid = ParseOid(oid_text);

			if(type_class == 'd')
				domain_types.Append({.oid = oid, .base_oid = ParseOid(PQgetvalue(result.get(), row, 4))});

			for(usys_t i = 0; i < registry.Count(); i++)
			{
				const IDatatypeCodec* const codec = registry[i];
				const bool namespace_matches = strcmp(codec->namespace_name, "*") == 0 || strcmp(codec->namespace_name, namespace_name) == 0;
				if(namespace_matches && strcmp(codec->datatype_name, datatype_name) == 0)
				{
					EL_ERROR(resolved_oids[i] != 0, TException, TString::Format("PostgreSQL datatype codec %s.%s matches more than one server datatype", codec->namespace_name, codec->datatype_name));
					resolved_oids[i] = oid;
					break;
				}
			}
		}

		TTypeMap type_map;
		for(usys_t i = 0; i < registry.Count(); i++)
		{
			const IDatatypeCodec* const codec = registry[i];
			const oid_t oid = resolved_oids[i];
			EL_ERROR(oid == 0 && codec->required, TException, TString::Format("PostgreSQL datatype %s.%s is not available on the server", codec->namespace_name, codec->datatype_name));
			if(oid == 0)
				continue;

			type_map.Add(oid, codec);

			bool has_preferred_codec = false;
			for(const TCodecBinding& binding : type_map.cxx_type_map)
				if(*binding.codec->cxx_type_info == *codec->cxx_type_info)
				{
					has_preferred_codec = true;
					break;
				}

			if(!has_preferred_codec)
				type_map.cxx_type_map.Append({.oid = oid, .codec = codec});
		}

		bool mapped_domain;
		do
		{
			mapped_domain = false;
			for(const TDomainType& domain : domain_types)
			{
				if(type_map.Get(domain.oid) != nullptr)
					continue;

				const IDatatypeCodec* const* const base_codec = type_map.Get(domain.base_oid);
				if(base_codec != nullptr)
				{
					type_map.Add(domain.oid, *base_codec);
					mapped_domain = true;
				}
			}
		}
		while(mapped_domain);

		return type_map;
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

	TPostgresColumnDescription::TPostgresColumnDescription() : codec(nullptr), is_null(true), buffer{}
	{
		type = nullptr;
	}

	TPostgresColumnDescription::~TPostgresColumnDescription()
	{
		if(!is_null)
		{
			codec->Destruct(buffer);
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
			const oid_t oid = (oid_t)PQftype(current_result.get(), i);
			const IDatatypeCodec& codec = type_map.LookupByOid(oid);
			EL_ERROR(codec.cxx_size > sizeof(d.buffer), TException, TString::Format("C++ datatype %q for PostgreSQL datatype %s.%s requires %d bytes, but result fields only provide %d bytes", debug::Demangle(codec.cxx_type_info->name()), codec.namespace_name, codec.datatype_name, (u64_t)codec.cxx_size, (u64_t)sizeof(d.buffer)));
			EL_ERROR(codec.cxx_alignment > ALIGN_FIELD_BUFFER, TException, TString::Format("C++ datatype %q for PostgreSQL datatype %s.%s requires alignment %d, but result fields only provide alignment %d", debug::Demangle(codec.cxx_type_info->name()), codec.namespace_name, codec.datatype_name, (u64_t)codec.cxx_alignment, (u64_t)ALIGN_FIELD_BUFFER));
			d.codec = &codec;
			d.type = codec.cxx_type_info;
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

				#ifdef LIBPQ_HAS_CHUNK_MODE
				case PGRES_TUPLES_CHUNK:
					EL_THROW(TLogicException);
				#endif

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

			if(!d.is_null)
			{
				d.codec->Destruct(d.buffer);
				d.is_null = true;
			}

			if(current_result != nullptr && PQgetisnull(current_result.get(), 0, i) == 0)
			{
				const byte_t* const value = reinterpret_cast<const byte_t*>(PQgetvalue(current_result.get(), 0, i));
				EL_ERROR(value == nullptr, TLogicException);
				const usys_t value_size = (usys_t)PQgetlength(current_result.get(), 0, i);
				d.codec->Deserialize(array_t<const byte_t>(value, value_size), d.buffer);
				d.is_null = false;
			}
		}
	}

	void TResultStream::MoveNext()
	{
		DeserializeResult(InternalMoveNext());
	}

	TResultStream::TResultStream(TPostgresConnection* const conn, TString sql) : conn(conn), type_map(conn->type_map), sql(std::move(sql))
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
							#ifdef LIBPQ_HAS_CHUNK_MODE
							case PGRES_TUPLES_CHUNK:
								EL_THROW(TLogicException);
							#endif
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

		type_map = TTypeMap::LoadTypeMap(*this);

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
		type_map.Clear();
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
		EL_ERROR(args.Count() > (usys_t)std::numeric_limits<int>::max(), TInvalidArgumentException, "args", "too many query arguments");

		const usys_t n_args = args.Count();
		auto oids = std::make_unique<Oid[]>(n_args);
		auto values = std::make_unique<TList<byte_t>[]>(n_args);
		auto value_ptrs = std::make_unique<const char*[]>(n_args);
		auto lengths = std::make_unique<int[]>(n_args);
		auto formats = std::make_unique<int[]>(n_args);

		for(usys_t i = 0; i < n_args; i++)
		{
			const query_arg_t& arg = args[i];

			EL_ERROR(arg.value != nullptr && arg.type == nullptr, TInvalidArgumentException, "args", "type must not be NULL when value is set");

			if(arg.value != nullptr)
			{
				const TTypeMap::TCodecBinding& binding = type_map.LookupByCxxType(*arg.type);
				const IDatatypeCodec& codec = *binding.codec;
				EL_ERROR(arg.sz_bytes < 0 || (usys_t)arg.sz_bytes != codec.cxx_size, TInvalidArgumentException, "args", "query argument size does not match its registered datatype codec");

				oids[i] = (Oid)binding.oid;
				const usys_t estimated_size = codec.Serialize(arg.value, array_t<byte_t>());
				EL_ERROR(estimated_size > (usys_t)std::numeric_limits<int>::max(), TException, "encoded PostgreSQL query argument exceeds the libpq size limit");
				values[i].SetCount(estimated_size == 0 ? 1U : estimated_size);
				const usys_t encoded_size = codec.Serialize(arg.value, values[i]);
				EL_ERROR(encoded_size > estimated_size, TLogicException);
				value_ptrs[i] = reinterpret_cast<const char*>(values[i].ItemPtr(0));
				lengths[i] = (int)encoded_size;
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

		EL_ERROR(PQsendQueryParams((PGconn*)pg_connection, sql.MakeCStr().get(), (int)n_args, oids.get(), value_ptrs.get(), lengths.get(), formats.get(), 1) != 1, TPostgresException, pg_connection, "unable to dispatch query to server");

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
