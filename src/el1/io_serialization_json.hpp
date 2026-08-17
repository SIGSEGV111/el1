#pragma once

#include "io_serialization.hpp"
#include "io_format_json.hpp"
#include <cmath>

namespace el1::io::serialization::json
{
	using format::json::TJsonArray;
	using format::json::TJsonMap;
	using format::json::TJsonValue;

	inline constexpr u32_t FORMAT_VERSION = 1;
	inline constexpr TStringView METADATA_KEY = U"$el1";

	class TWriter
	{
		TJsonValue root;
		TJsonValue* current = &root;
		io::collection::list::TList<TJsonValue*> parents;

		void Push(TJsonValue& child)
		{
			parents.Append(current);
			current = &child;
		}

		void Pop()
		{
			EL_ERROR(parents.Count() == 0, TLogicException);
			current = parents[-1];
			parents.Remove(-1);
		}

	public:
		void BeginOptional(const bool present) { if(!present) current->SetNull(); }
		void EndOptional() {}
		void Boolean(const bool value) { *current = TJsonValue(value); }
		void Signed(const s64_t value) { *current = TJsonValue(value); }
		void Unsigned(const u64_t value) { *current = TJsonValue(value); }
		void Floating(const double value)
		{
			EL_ERROR(!std::isfinite(value), TException, U"JSON serialization does not support NaN or infinity");
			*current = TJsonValue(value);
		}
		void String(const TStringView value) { *current = TJsonValue(TString(value)); }

		void BeginObject(const TTypeInfo& info)
		{
			*current = TJsonValue(TJsonMap());
			TJsonMap metadata;
			metadata.Add(TString(U"format"), TJsonValue((s64_t)FORMAT_VERSION));
			metadata.Add(TString(U"type"), TJsonValue(TString(info.name)));
			metadata.Add(TString(U"type_id"), TJsonValue(info.id.ToString()));
			metadata.Add(TString(U"version"), TJsonValue((s64_t)info.version));
			current->Map().Add(TString(METADATA_KEY), TJsonValue(std::move(metadata)));
		}
		void EndObject() {}

		void BeginField(const TFieldInfo& info)
		{
			auto& child = current->Map().Add(TString(info.name), TJsonValue());
			Push(child);
		}
		void EndField() { Pop(); }

		void BeginArray(const usys_t count)
		{
			TJsonArray array;
			array.Clear(count);
			*current = TJsonValue(std::move(array));
		}
		void BeginElement(const usys_t)
		{
			auto& child = current->Array().Append(TJsonValue());
			Push(child);
		}
		void EndElement() { Pop(); }
		void EndArray() {}

		void BeginMap(const usys_t) { *current = TJsonValue(TJsonMap()); }
		void BeginMapEntry(const usys_t, const TStringView key)
		{
			auto& child = current->Map().Add(TString(key), TJsonValue());
			Push(child);
		}
		void EndMapEntry() { Pop(); }
		void EndMap() {}

		TJsonValue TakeResult()
		{
			EL_ERROR(parents.Count() != 0, TLogicException);
			return std::move(root);
		}
	};

	class TReader
	{
		const TJsonValue* current;
		io::collection::list::TList<const TJsonValue*> parents;
		TDeserializeOptions options;
		usys_t depth = 0;

		void Push(const TJsonValue& child)
		{
			EL_ERROR(depth >= options.max_depth, TException, U"maximum serialization nesting depth exceeded");
			parents.Append(current);
			current = &child;
			depth++;
		}

		void Pop()
		{
			EL_ERROR(parents.Count() == 0 || depth == 0, TLogicException);
			current = parents[-1];
			parents.Remove(-1);
			depth--;
		}

		const format::json::TConstJsonMap& RequireMap() const
		{
			EL_ERROR(!current->IsMap(), TException, U"expected JSON object during deserialization");
			return current->Map();
		}

		template<std::integral T>
		static T RequireInteger(const TJsonValue& value)
		{
			return value.ToInteger<T>();
		}

	public:
		explicit TReader(const TJsonValue& root, const TDeserializeOptions& options = {}) : current(&root), options(options) {}

		bool BeginOptional() const { return !current->IsNull(); }
		void EndOptional() {}

		bool Boolean()
		{
			EL_ERROR(!current->IsBoolean(), TException, U"expected JSON boolean during deserialization");
			return current->Boolean();
		}

		s64_t Signed()
		{
			return RequireInteger<s64_t>(*current);
		}

		u64_t Unsigned()
		{
			return RequireInteger<u64_t>(*current);
		}

		double Floating()
		{
			EL_ERROR(!current->IsNumber(), TException, U"expected JSON number during deserialization");
			return current->Number();
		}

		TString String()
		{
			EL_ERROR(!current->IsString(), TException, U"expected JSON string during deserialization");
			EL_ERROR(current->String().Length() > options.max_string_length, TException, U"maximum serialized string length exceeded");
			return current->String();
		}

		u32_t BeginObject(const TTypeInfo& expected)
		{
			const auto& object = RequireMap();
			EL_ERROR(!object.Contains(TString(METADATA_KEY)), TException, U"serialized object is missing $el1 metadata");
			const TJsonValue& metadata_value = object[TString(METADATA_KEY)];
			EL_ERROR(!metadata_value.IsMap(), TException, U"serialized $el1 metadata must be an object");
			const auto& metadata = metadata_value.Map();
			const TString format_key(U"format");
			const TString type_id_key(U"type_id");
			const TString version_key(U"version");
			EL_ERROR(!metadata.Contains(format_key) || !metadata.Contains(type_id_key) || !metadata.Contains(version_key), TException, U"serialized object metadata is incomplete");
			EL_ERROR(RequireInteger<s64_t>(metadata[format_key]) != (s64_t)FORMAT_VERSION, TException, U"unsupported serialization JSON format version");
			EL_ERROR(!metadata[type_id_key].IsString(), TException, U"serialized type_id must be a string");
			EL_ERROR(TTypeId::FromString(metadata[type_id_key].String().View()) != expected.id, TException, U"serialized type does not match requested C++ type");
			const s64_t version = RequireInteger<s64_t>(metadata[version_key]);
			EL_ERROR(version <= 0 || (u64_t)version > expected.version, TException, U"serialized schema version is not supported by this C++ type");
			return (u32_t)version;
		}
		void EndObject() {}

		bool BeginField(const TFieldInfo& info)
		{
			const auto& object = RequireMap();
			const TString key(info.name);
			if(!object.Contains(key))
				return false;
			Push(object[key]);
			return true;
		}
		void EndField() { Pop(); }

		usys_t BeginArray()
		{
			EL_ERROR(!current->IsArray(), TException, U"expected JSON array during deserialization");
			const usys_t count = current->Array().Count();
			EL_ERROR(count > options.max_container_items, TException, U"maximum serialized container size exceeded");
			return count;
		}
		void BeginElement(const usys_t index)
		{
			EL_ERROR(!current->IsArray() || index >= current->Array().Count(), TLogicException);
			Push(current->Array()[(ssys_t)index]);
		}
		void EndElement() { Pop(); }
		void EndArray() {}

		usys_t BeginMap()
		{
			EL_ERROR(!current->IsMap(), TException, U"expected JSON object/map during deserialization");
			const usys_t count = current->Map().Items().Count();
			EL_ERROR(count > options.max_container_items, TException, U"maximum serialized map size exceeded");
			return count;
		}
		TString BeginMapEntry(const usys_t index)
		{
			EL_ERROR(!current->IsMap() || index >= current->Map().Items().Count(), TLogicException);
			const auto& item = current->Map().Items()[index];
			TString key = item.key;
			Push(item.value);
			return key;
		}
		void EndMapEntry() { Pop(); }
		void EndMap() {}
	};

	template<typename T>
	TJsonValue ToValue(const T& value)
	{
		TWriter writer;
		serialization::Serialize(writer, value);
		return writer.TakeResult();
	}

	template<typename T>
	void FromValue(const TJsonValue& source, T& target, const TDeserializeOptions& options = {})
	{
		TReader reader(source, options);
		serialization::Deserialize(reader, target);
	}

	template<typename T>
	T FromValue(const TJsonValue& source, const TDeserializeOptions& options = {})
	{
		T target{};
		FromValue(source, target, options);
		return target;
	}

	template<typename T>
	TString ToString(const T& value)
	{
		return ToValue(value).ToString();
	}

	template<typename T>
	T FromString(const TStringView text, const TDeserializeOptions& options = {})
	{
		return FromValue<T>(TJsonValue::Parse(text), options);
	}
}
