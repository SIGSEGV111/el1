#pragma once

#include "io_types.hpp"
#include "util_function.hpp"
#include <typeinfo>

namespace el1::io::text::string
{
	class TString;
}

namespace el1::io::serialization
{
	// TODO version numbers

	using namespace types;

	class IObjectSerializer;
	class IArraySerializer;

	class IObjectSerializer
	{
		private:
			virtual void BeginObject(const std::type_info& tid, const char* const key) = 0;
			virtual void EndObject(const std::type_info& tid, const char* const key) = 0;

		public:
			virtual void Serialize(const char* const key, const u8_t value) = 0;
			virtual void Serialize(const char* const key, const s8_t value) = 0;
			virtual void Serialize(const char* const key, const u16_t value) = 0;
			virtual void Serialize(const char* const key, const s16_t value) = 0;
			virtual void Serialize(const char* const key, const u32_t value) = 0;
			virtual void Serialize(const char* const key, const s32_t value) = 0;
			virtual void Serialize(const char* const key, const u64_t value) = 0;
			virtual void Serialize(const char* const key, const s64_t value) = 0;
			virtual void Serialize(const char* const key, const float value) = 0;
			virtual void Serialize(const char* const key, const double value) = 0;
			virtual void Serialize(const char* const key, const text::string::TString& value) = 0;

			template<typename T>
			void SerializeObject(const char* key, const T& object)
			{
				this->BeginObject(typeid(T), key);
				object.Serialize(*this);
				this->EndObject(typeid(T), key);
			}

			virtual void SerializeArray(const char* key, util::function::TFunction<void, IArraySerializer&> lambda) = 0;
	};

	class IArraySerializer
	{
		private:
			virtual void SerializeObject(const std::type_info& tid, util::function::TFunction<void, IObjectSerializer&> lambda) = 0;

		public:
			virtual void Serialize(const u8_t value) = 0;
			virtual void Serialize(const s8_t value) = 0;
			virtual void Serialize(const u16_t value) = 0;
			virtual void Serialize(const s16_t value) = 0;
			virtual void Serialize(const u32_t value) = 0;
			virtual void Serialize(const s32_t value) = 0;
			virtual void Serialize(const u64_t value) = 0;
			virtual void Serialize(const s64_t value) = 0;
			virtual void Serialize(const float value) = 0;
			virtual void Serialize(const double value) = 0;
			virtual void Serialize(const text::string::TString& value) = 0;

			template<typename T>
			void SerializeObject(const T& object)
			{
				this->SerializeObject(typeid(T), util::function::TFunction<void, IObjectSerializer&>(&object, &T::Serialize));
			}

			virtual void SerializeArray(const char* key, util::function::TFunction<void, IArraySerializer&> lambda) = 0;
	};

	#define EL_S7EVAR(serializer, variable) serializer.Serialize(#variable, variable)
	#define EL_D9EVAR(deserializer, variable) deserializer.Deserialize(#variable, variable)
}
