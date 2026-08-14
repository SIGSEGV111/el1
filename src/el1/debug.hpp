#pragma once

#include "io_types.hpp"

namespace el1::io::text::string
{
	class TString;
}

namespace el1::debug
{
	using namespace io::types;

	struct TTypeNameView
	{
		const char* const data;
		const usys_t length;
	};

	template<typename T>
	constexpr TTypeNameView GetTypeName() noexcept
	{
		constexpr const char* const signature = __PRETTY_FUNCTION__;
		constexpr usys_t signature_length = sizeof(__PRETTY_FUNCTION__) - 1U;
		constexpr char marker[] = "T = ";
		constexpr usys_t marker_length = sizeof(marker) - 1U;

		usys_t begin = 0;
		bool found = false;
		for(; begin + marker_length <= signature_length; begin++)
		{
			usys_t i = 0;
			for(; i < marker_length && signature[begin + i] == marker[i]; i++);
			if(i == marker_length)
			{
				begin += marker_length;
				found = true;
				break;
			}
		}

		if(!found)
			return TTypeNameView{signature, signature_length};

		const usys_t end = signature[signature_length - 1U] == ']' ? signature_length - 1U : signature_length;
		return TTypeNameView{signature + begin, end - begin};
	}

	void Hexdump(const char* const context, const void* const data, const usys_t n_bytes, const usys_t line_length = 16);
	void Hexdump(const void* const data, const usys_t n_bytes, const usys_t line_length = 16, const char* const context = "");
	io::text::string::TString HexdumpStr(const void* const data, const usys_t n_bytes, const usys_t line_length = 16, const char* const context = "");

	io::text::string::TString Demangle(const char*);
}
