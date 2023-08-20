#pragma once

#include "io_types.hpp"

namespace el1::io::text::string
{
	class TString;
}

namespace el1::debug
{
	using namespace io::types;

	void Hexdump(const char* const context, const void* const data, const usys_t n_bytes, const usys_t line_length = 16);
	void Hexdump(const void* const data, const usys_t n_bytes, const usys_t line_length = 16, const char* const context = "");
	io::text::string::TString HexdumpStr(const void* const data, const usys_t n_bytes, const usys_t line_length = 16, const char* const context = "");
}
