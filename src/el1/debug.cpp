#include "debug.hpp"
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include "io_text_string.hpp"

namespace el1::debug
{
	using namespace io::text::string;

	template<typename L>
	void Hexdump(const void* const data, const usys_t n_bytes, const usys_t line_length, const char* const context, L write_line)
	{
		const byte_t* const arr_bytes = (const byte_t*)data;

		const unsigned buffer_size = strlen(context) + 1 + sizeof(void*) * 2 + 2 + 3 * line_length + 1 + line_length + 1 + 1;
		char line_buffer[buffer_size];

		for(usys_t i = 0; i < n_bytes; i+= line_length)
		{
			unsigned pos = 0;
			pos += snprintf(line_buffer + pos, buffer_size - pos, "%s[%04zx]", context, (size_t)i);

			for(usys_t j = 0; j < line_length; j++)
			{
				const usys_t k = i + j;
				if(k < n_bytes)
					pos += snprintf(line_buffer + pos, buffer_size - pos, " %02hhx", arr_bytes[k]);
				else
					pos += snprintf(line_buffer + pos, buffer_size - pos, "   ");
			}

			pos += snprintf(line_buffer + pos, buffer_size - pos, " |");

			for(usys_t j = 0; j < line_length; j++)
			{
				const usys_t k = i + j;
				if(k < n_bytes)
					pos += snprintf(line_buffer + pos, buffer_size - pos, "%c", (isprint(arr_bytes[k]) != 0 ? arr_bytes[k] : ' '));
				else
					pos += snprintf(line_buffer + pos, buffer_size - pos, " ");
			}

			pos += snprintf(line_buffer + pos, buffer_size - pos, "|");
			// fprintf(stderr, "pos = %u, buffer_size = %u\n", pos, buffer_size);
			write_line(line_buffer);
		}
	}

	void Hexdump(const char* const context, const void* const data, const usys_t n_bytes, const usys_t line_length)
	{
		Hexdump(data, n_bytes, line_length, context);
	}

	void Hexdump(const void* const data, const usys_t n_bytes, const usys_t line_length, const char* const context)
	{
		Hexdump(data, n_bytes, line_length, context, [](const char* const line) {
			fputs(line, stderr);
			fputc('\n', stderr);
		});
	}

	TString HexdumpStr(const void* const data, const usys_t n_bytes, const usys_t line_length, const char* const context)
	{
		TString str;
		Hexdump(data, n_bytes, line_length, context, [&](const char* const line) {
			str += line;
			str += '\n';
		});
		return str;
	}
}
