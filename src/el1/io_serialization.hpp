#pragma once

#include "error.hpp"
#include "io_types.hpp"
#include "io_text_string.hpp"
#include "io_collection_array.hpp"
#include "io_collection_list.hpp"
#include "io_collection_map.hpp"

#include <concepts>
#include <limits>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace el1::io::serialization
{
	using namespace io::types;
	using io::text::string::TString;
	using io::text::string::TStringView;

	inline constexpr u32_t SCHEMA_VERSION_NONE = 0;
	inline constexpr u32_t SCHEMA_VERSION_CURRENT = std::numeric_limits<u32_t>::max();

	struct TTypeId
	{
		u64_t high = 0;
		u64_t low = 0;

		constexpr bool operator==(const TTypeId&) const noexcept = default;
		constexpr bool operator!=(const TTypeId&) const noexcept = default;

		static consteval TTypeId FromName(const TStringView name)
		{
			u64_t h1 = 1469598103934665603ULL;
			u64_t h2 = 1099511628211ULL ^ 0x9e3779b97f4a7c15ULL;
			for(const char32_t chr : name)
			{
				const u32_t value = (u32_t)chr;
				for(unsigned shift = 0; shift < 32; shift += 8)
				{
					const u8_t byte = (u8_t)(value >> shift);
					h1 = (h1 ^ byte) * 1099511628211ULL;
					h2 = (h2 ^ (byte + 0x5bU)) * 0x100000001b3ULL;
				}
			}
			return {h1, h2};
		}

		TString ToString() const;
		static TTypeId FromString(const TStringView text);
	};

	enum class ETypeKind : u8_t
	{
		OBJECT,
	};

	struct TTypeInfo
	{
		TTypeId id;
		TStringView name;
		u32_t version;
		ETypeKind kind;
	};

	enum class EFieldFlags : u8_t
	{
		NONE = 0,
	};

	struct TFieldInfo
	{
		u32_t id;
		TStringView name;
		u32_t since_version;
		u32_t until_version;
		EFieldFlags flags;

		constexpr bool Active(const u32_t version) const noexcept
		{
			return version >= since_version && version <= until_version;
		}
	};

	class TTypeRegistry; // reserved for polymorphic/object-factory support in serialization v2

	struct TDeserializeOptions
	{
		usys_t max_depth = 64;
		usys_t max_string_length = 16U * 1024U * 1024U;
		usys_t max_container_items = 16U * 1024U * 1024U;
		const TTypeRegistry* registry = nullptr; // ignored in v1; API reservation for v2
	};

	template<typename T>
	struct TSchema;

	template<typename T>
	struct TCodec;

	constexpr u32_t FieldId(const TStringView name) noexcept
	{
		u32_t hash = 2166136261U;
		for(const char32_t chr : name)
		{
			const u32_t value = (u32_t)chr;
			for(unsigned shift = 0; shift < 32; shift += 8)
				hash = (hash ^ (u8_t)(value >> shift)) * 16777619U;
		}
		return hash;
	}

	template<typename TObject, typename TValue>
	struct TMember
	{
		TFieldInfo info;
		TValue TObject::*pointer;
	};

	template<typename TObject, typename TValue>
	constexpr auto Member(
		const TStringView name,
		TValue TObject::* const pointer,
		const u32_t since_version = 1,
		const u32_t until_version = SCHEMA_VERSION_CURRENT,
		const u32_t id = 0)
	{
		return TMember<TObject, TValue>{
			TFieldInfo{id == 0 ? FieldId(name) : id, name, since_version, until_version, EFieldFlags::NONE},
			pointer
		};
	}

	template<typename T>
	concept CHasSchema = requires
	{
		{ TSchema<T>::Info() } -> std::same_as<TTypeInfo>;
		TSchema<T>::Members();
	};

	template<typename T> struct TIsArrayView : std::false_type {};
	template<typename T> struct TIsArrayView<io::collection::array::array_t<T>> : std::true_type {};

	template<typename T>
	inline constexpr bool REQUIRES_EXPLICIT_CODEC =
		std::is_pointer_v<std::remove_cv_t<T>> ||
		std::is_array_v<std::remove_cv_t<T>> ||
		std::same_as<std::remove_cv_t<T>, TStringView> ||
		TIsArrayView<std::remove_cv_t<T>>::value;

	namespace detail
	{
		template<typename T> struct TIsList : std::false_type {};
		template<typename T> struct TIsList<io::collection::list::TList<T>> : std::true_type { using value_t = T; };

		template<typename T> struct TIsStringMap : std::false_type {};
		template<typename V> struct TIsStringMap<io::collection::map::TSortedMap<TString, V>> : std::true_type { using value_t = V; };

		template<typename T> struct TIsOptional : std::false_type {};
		template<typename V> struct TIsOptional<std::optional<V>> : std::true_type { using value_t = V; };

		template<typename T> inline constexpr bool ALWAYS_FALSE = false;

		template<typename T>
		using clean_t = std::remove_cv_t<std::remove_reference_t<T>>;

		template<typename TArchive, typename T>
		concept CHasCustomSerialize = requires(TArchive& archive, const T& value)
		{
			TCodec<T>::Serialize(archive, value);
		};

		template<typename TArchive, typename T>
		concept CHasCustomDeserialize = requires(TArchive& archive, T& value)
		{
			TCodec<T>::Deserialize(archive, value);
		};
	}

	template<typename TArchive, typename T>
	void Serialize(TArchive& archive, const T& value);

	template<typename TArchive, typename T>
	void Deserialize(TArchive& archive, T& value);

	template<typename TArchive, typename T>
	void Serialize(TArchive& archive, const T& value)
	{
		using U = detail::clean_t<T>;
		if constexpr(detail::CHasCustomSerialize<TArchive, U>)
			TCodec<U>::Serialize(archive, value);
		else if constexpr(std::same_as<U, bool>)
			archive.Boolean(value);
		else if constexpr(std::is_enum_v<U>)
			Serialize(archive, (std::underlying_type_t<U>)value);
		else if constexpr(std::is_integral_v<U>)
		{
			if constexpr(std::is_signed_v<U>) archive.Signed((s64_t)value);
			else archive.Unsigned((u64_t)value);
		}
		else if constexpr(std::is_floating_point_v<U>)
			archive.Floating((double)value);
		else if constexpr(std::same_as<U, TString>)
			archive.String(value.View());
		else if constexpr(detail::TIsOptional<U>::value)
		{
			archive.BeginOptional(value.has_value());
			if(value) Serialize(archive, *value);
			archive.EndOptional();
		}
		else if constexpr(detail::TIsStringMap<U>::value)
		{
			archive.BeginMap(value.Items().Count());
			usys_t index = 0;
			for(const auto& item : value.Items())
			{
				archive.BeginMapEntry(index++, item.key.View());
				Serialize(archive, item.value);
				archive.EndMapEntry();
			}
			archive.EndMap();
		}
		else if constexpr(detail::TIsList<U>::value)
		{
			archive.BeginArray(value.Count());
			for(usys_t i = 0; i < value.Count(); i++)
			{
				archive.BeginElement(i);
				Serialize(archive, value[i]);
				archive.EndElement();
			}
			archive.EndArray();
		}
		else if constexpr(CHasSchema<U>)
		{
			const TTypeInfo info = TSchema<U>::Info();
			archive.BeginObject(info);
			std::apply([&](const auto&... member)
			{
				([&]
				{
					if(member.info.Active(info.version))
					{
						archive.BeginField(member.info);
						Serialize(archive, value.*(member.pointer));
						archive.EndField();
					}
				}(), ...);
			}, TSchema<U>::Members());
			archive.EndObject();
		}
		else if constexpr(REQUIRES_EXPLICIT_CODEC<U>)
			static_assert(detail::ALWAYS_FALSE<U>, "serialization of pointers, views and C arrays requires an explicit TCodec<T>");
		else
			static_assert(detail::ALWAYS_FALSE<U>, "type is not serializable: define TSchema<T> or TCodec<T>");
	}

	template<typename TArchive, typename T>
	void Deserialize(TArchive& archive, T& value)
	{
		using U = detail::clean_t<T>;
		if constexpr(detail::CHasCustomDeserialize<TArchive, U>)
			TCodec<U>::Deserialize(archive, value);
		else if constexpr(std::same_as<U, bool>)
			value = archive.Boolean();
		else if constexpr(std::is_enum_v<U>)
		{
			std::underlying_type_t<U> raw{};
			Deserialize(archive, raw);
			value = (U)raw;
		}
		else if constexpr(std::is_integral_v<U>)
		{
			if constexpr(std::is_signed_v<U>)
			{
				const s64_t raw = archive.Signed();
				EL_ERROR(raw < (s64_t)std::numeric_limits<U>::min() || raw > (s64_t)std::numeric_limits<U>::max(), TException, "serialized integer outside target range");
				value = (U)raw;
			}
			else
			{
				const u64_t raw = archive.Unsigned();
				EL_ERROR(raw > (u64_t)std::numeric_limits<U>::max(), TException, "serialized integer outside target range");
				value = (U)raw;
			}
		}
		else if constexpr(std::is_floating_point_v<U>)
			value = (U)archive.Floating();
		else if constexpr(std::same_as<U, TString>)
			value = archive.String();
		else if constexpr(detail::TIsOptional<U>::value)
		{
			if(archive.BeginOptional())
			{
				value.emplace();
				Deserialize(archive, *value);
			}
			else value.reset();
			archive.EndOptional();
		}
		else if constexpr(detail::TIsStringMap<U>::value)
		{
			const usys_t count = archive.BeginMap();
			value.Clear();
			for(usys_t i = 0; i < count; i++)
			{
				const TString key = archive.BeginMapEntry(i);
				typename detail::TIsStringMap<U>::value_t item{};
				Deserialize(archive, item);
				archive.EndMapEntry();
				value.Add(key, std::move(item));
			}
			archive.EndMap();
		}
		else if constexpr(detail::TIsList<U>::value)
		{
			const usys_t count = archive.BeginArray();
			value.Clear(count);
			for(usys_t i = 0; i < count; i++)
			{
				archive.BeginElement(i);
				typename detail::TIsList<U>::value_t item{};
				Deserialize(archive, item);
				value.Append(std::move(item));
				archive.EndElement();
			}
			archive.EndArray();
		}
		else if constexpr(CHasSchema<U>)
		{
			const TTypeInfo info = TSchema<U>::Info();
			const u32_t source_version = archive.BeginObject(info);
			std::apply([&](const auto&... member)
			{
				([&]
				{
					if(member.info.Active(source_version) && archive.BeginField(member.info))
					{
						Deserialize(archive, value.*(member.pointer));
						archive.EndField();
					}
				}(), ...);
			}, TSchema<U>::Members());
			archive.EndObject();
		}
		else if constexpr(REQUIRES_EXPLICIT_CODEC<U>)
			static_assert(detail::ALWAYS_FALSE<U>, "deserialization of pointers, views and C arrays requires an explicit TCodec<T>");
		else
			static_assert(detail::ALWAYS_FALSE<U>, "type is not deserializable: define TSchema<T> or TCodec<T>");
	}
}

#define EL_SERIALIZATION_STRINGIFY_IMPL(x) #x
#define EL_SERIALIZATION_STRINGIFY(x) EL_SERIALIZATION_STRINGIFY_IMPL(x)
#define EL_SERIALIZATION_U32_IMPL(x) U##x
#define EL_SERIALIZATION_U32(x) EL_SERIALIZATION_U32_IMPL(x)
#define EL_SERIALIZATION_USTRING(x) EL_SERIALIZATION_U32(EL_SERIALIZATION_STRINGIFY(x))

// Declare a member of the TObject bound by the surrounding EL_SERIALIZABLE().
// Optional arguments are forwarded to Member() as since_version, until_version and id.
#define EL_SERIALIZATION_MEMBER(MEMBER, ...) \
	::el1::io::serialization::Member(EL_SERIALIZATION_USTRING(MEMBER), &TObject::MEMBER __VA_OPT__(,) __VA_ARGS__)

// Define the schema and bind TYPE once as TObject for all member declarations below.
#define EL_SERIALIZABLE(TYPE, VERSION, ...) \
	template<> struct el1::io::serialization::TSchema<TYPE> \
	{ \
		using TObject = TYPE; \
		static constexpr ::el1::io::serialization::TTypeInfo Info() \
		{ \
			constexpr ::el1::io::text::string::TStringView name = EL_SERIALIZATION_USTRING(TYPE); \
			return { ::el1::io::serialization::TTypeId::FromName(name), name, VERSION, ::el1::io::serialization::ETypeKind::OBJECT }; \
		} \
		static constexpr auto Members() { return std::tuple{ __VA_ARGS__ }; } \
	}
