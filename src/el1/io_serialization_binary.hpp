#pragma once

#include "io_serialization.hpp"
#include "io_stream.hpp"
#include "io_text_encoding_utf8.hpp"
#include "io_collection_list.hpp"

#include <bit>
#include <limits>

namespace el1::io::serialization::binary::packed
{
	using namespace io::stream;
	using namespace io::collection::list;
	using namespace io::text::encoding::utf8;

	inline constexpr u32_t FORMAT_VERSION = 1;

	class TWriter
	{
		IBinarySink* sink;

		void Byte(const byte_t value) { sink->WriteAll(&value, 1); }

		void VarUInt(u64_t value)
		{
			while(value >= 0x80)
			{
				Byte((byte_t)(value | 0x80));
				value >>= 7;
			}
			Byte((byte_t)value);
		}

		void Fixed64(u64_t value)
		{
			byte_t data[8];
			for(unsigned i = 0; i < 8; i++)
				data[i] = (byte_t)(value >> (i * 8));
			sink->WriteAll(data, 8);
		}

	public:
		explicit TWriter(IBinarySink* const sink EL_LIFETIME_BOUND) : sink(sink)
		{
			static constexpr byte_t MAGIC[] = {'E', 'L', '1', 'S'};
			sink->WriteAll(MAGIC, sizeof(MAGIC));
			VarUInt(FORMAT_VERSION);
		}

		void BeginOptional(const bool present) { Byte(present ? 1 : 0); }
		void EndOptional() {}
		void Boolean(const bool value) { Byte(value ? 1 : 0); }
		void Signed(const s64_t value)
		{
			const u64_t zigzag = value >= 0 ? (u64_t)value * 2U : (u64_t)(-(value + 1)) * 2U + 1U;
			VarUInt(zigzag);
		}
		void Unsigned(const u64_t value) { VarUInt(value); }
		void Floating(const double value) { Fixed64(std::bit_cast<u64_t>(value)); }

		void String(const TStringView value)
		{
			TString copy(value);
			TList<byte_t> bytes = copy.chars.Pipe().Transform(TUTF8Encoder()).Collect();
			VarUInt(bytes.Count());
			if(bytes.Count() != 0)
				sink->WriteAll(bytes.Data(), bytes.Count());
		}

		void BeginObject(const TTypeInfo& info)
		{
			Fixed64(info.id.high);
			Fixed64(info.id.low);
			VarUInt(info.version);
		}
		void EndObject() {}
		void BeginField(const TFieldInfo&) {}
		void EndField() {}
		void BeginArray(const usys_t count) { VarUInt(count); }
		void BeginElement(const usys_t) {}
		void EndElement() {}
		void EndArray() {}
		void BeginMap(const usys_t count) { VarUInt(count); }
		void BeginMapEntry(const usys_t, const TStringView key) { String(key); }
		void EndMapEntry() {}
		void EndMap() {}
	};

	class TReader
	{
		IBinarySource* source;
		TDeserializeOptions options;
		usys_t depth = 0;

		byte_t Byte()
		{
			byte_t value;
			source->ReadAll(&value, 1);
			return value;
		}

		u64_t VarUInt()
		{
			u64_t value = 0;
			for(unsigned shift = 0; shift < 64; shift += 7)
			{
				const byte_t byte = Byte();
				value |= (u64_t)(byte & 0x7f) << shift;
				if((byte & 0x80) == 0)
					return value;
			}
			EL_THROW(TException, "invalid packed serialization varint");
		}

		u64_t Fixed64()
		{
			byte_t data[8];
			source->ReadAll(data, 8);
			u64_t value = 0;
			for(unsigned i = 0; i < 8; i++)
				value |= (u64_t)data[i] << (i * 8);
			return value;
		}

		void Enter()
		{
			EL_ERROR(depth >= options.max_depth, TException, "maximum serialization nesting depth exceeded");
			depth++;
		}
		void Leave()
		{
			EL_ERROR(depth == 0, TLogicException);
			depth--;
		}

	public:
		explicit TReader(IBinarySource* const source EL_LIFETIME_BOUND, const TDeserializeOptions& options = {}) : source(source), options(options)
		{
			byte_t magic[4];
			source->ReadAll(magic, 4);
			EL_ERROR(magic[0] != 'E' || magic[1] != 'L' || magic[2] != '1' || magic[3] != 'S', TException, "invalid packed serialization magic");
			EL_ERROR(VarUInt() != FORMAT_VERSION, TException, "unsupported packed serialization format version");
		}

		bool BeginOptional()
		{
			const byte_t tag = Byte();
			EL_ERROR(tag > 1, TException, "invalid packed optional tag");
			return tag != 0;
		}
		void EndOptional() {}

		bool Boolean()
		{
			const byte_t value = Byte();
			EL_ERROR(value > 1, TException, "invalid packed boolean");
			return value != 0;
		}
		s64_t Signed()
		{
			const u64_t value = VarUInt();
			if((value & 1U) == 0)
				return (s64_t)(value >> 1);
			const u64_t magnitude_minus_one = value >> 1;
			return -(s64_t)magnitude_minus_one - 1;
		}
		u64_t Unsigned() { return VarUInt(); }
		double Floating() { return std::bit_cast<double>(Fixed64()); }

		TString String()
		{
			const u64_t count64 = VarUInt();
			EL_ERROR(count64 > options.max_string_length || count64 > (u64_t)std::numeric_limits<usys_t>::max(), TException, "maximum serialized string length exceeded");
			const usys_t count = (usys_t)count64;
			TList<byte_t> bytes;
			bytes.SetCount(count);
			if(count != 0)
				source->ReadAll(bytes.Data(), count);
			return bytes.Pipe().Transform(TUTF8Decoder()).Collect();
		}

		u32_t BeginObject(const TTypeInfo& expected)
		{
			Enter();
			const TTypeId actual{Fixed64(), Fixed64()};
			EL_ERROR(actual != expected.id, TException, "serialized type does not match requested C++ type");
			const u64_t version = VarUInt();
			EL_ERROR(version == 0 || version > expected.version, TException, "serialized schema version is not supported by this C++ type");
			return (u32_t)version;
		}
		void EndObject() { Leave(); }
		bool BeginField(const TFieldInfo&) { return true; }
		void EndField() {}
		usys_t BeginArray()
		{
			Enter();
			const u64_t count = VarUInt();
			EL_ERROR(count > options.max_container_items || count > (u64_t)std::numeric_limits<usys_t>::max(), TException, "maximum serialized container size exceeded");
			return (usys_t)count;
		}
		void BeginElement(const usys_t) {}
		void EndElement() {}
		void EndArray() { Leave(); }

		usys_t BeginMap()
		{
			Enter();
			const u64_t count = VarUInt();
			EL_ERROR(count > options.max_container_items || count > (u64_t)std::numeric_limits<usys_t>::max(), TException, "maximum serialized map size exceeded");
			return (usys_t)count;
		}
		TString BeginMapEntry(const usys_t) { return String(); }
		void EndMapEntry() {}
		void EndMap() { Leave(); }
	};

	template<typename T>
	void Serialize(IBinarySink& sink, const T& value)
	{
		TWriter writer(&sink);
		serialization::Serialize(writer, value);
	}

	template<typename T>
	T Deserialize(IBinarySource& source, const TDeserializeOptions& options = {})
	{
		TReader reader(&source, options);
		T value{};
		serialization::Deserialize(reader, value);
		return value;
	}

	template<typename T>
	TList<byte_t> ToBytes(const T& value)
	{
		TList<byte_t> bytes;
		TListSink<byte_t> sink(&bytes);
		Serialize(sink, value);
		return bytes;
	}

	template<typename T>
	T FromBytes(TList<byte_t> bytes, const TDeserializeOptions& options = {})
	{
		TListSource<byte_t> source(std::move(bytes));
		return Deserialize<T>(source, options);
	}
}
