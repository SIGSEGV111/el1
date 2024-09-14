#include "io_compression_deflate.hpp"
#include "io_text_string.hpp"
#include <zlib.h>

namespace el1::io::compression::deflate
{
	static const usys_t SZ_OUTBUF_INCR = 1048576;

	TList<byte_t> Inflate(array_t<const byte_t> input)
	{
		z_stream stream = {};
		EL_ERROR(inflateInit(&stream) != Z_OK, TException, "inflateInit() failed");

		try
		{
			TList<byte_t> output;
			usys_t idx_input = 0;
			usys_t idx_output = 0;
			int status;

			do
			{
				output.SetCount(idx_output + SZ_OUTBUF_INCR);

				stream.avail_in = input.Count() - idx_input;
				stream.next_in = (Bytef*)&input[idx_input];
				stream.avail_out = SZ_OUTBUF_INCR;
				stream.next_out = (Bytef*)&output[idx_output];

				status = inflate(&stream, Z_NO_FLUSH);
				EL_ERROR(status != Z_OK && status != Z_STREAM_END, TException, "inflate() failed");

				const usys_t n_consumed = (input.Count() - idx_input) - stream.avail_in;
				const usys_t n_produced = SZ_OUTBUF_INCR - stream.avail_out;
				idx_input += n_consumed;
				idx_output += n_produced;
			}
			while(status != Z_STREAM_END);

			inflateEnd(&stream);
			output.SetCount(idx_output);
			return output;
		}
		catch(...)
		{
			inflateEnd(&stream);
			throw;
		}
	}
}
