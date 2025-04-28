#pragma once

#include "io_stream.hpp"
#include "io_text_encoding.hpp"
#include "io_text_string.hpp"

namespace el1::io::format::base64
{
	using namespace types;
	using namespace text::encoding;
	using namespace text::string;

	class TBase64Decoder
	{
		protected:
			byte_t buffer[3];
			byte_t n_remaining : 4;
			byte_t index : 4;

		public:
			using TIn = TUTF32;
			using TOut = byte_t;

			template<typename TSourceStream>
			static byte_t ReadNextChar(TSourceStream* const source)
			{
				const TUTF32* const chr = source->NextItem();

				if(chr == nullptr)
					return 65;

				if(chr->code >= 'A' && chr->code <= 'Z')
					return chr->code - 'A' +  0;

				if(chr->code >= 'a' && chr->code <= 'z')
					return chr->code - 'a' + 26;

				if(chr->code >= '0' && chr->code <= '9')
					return chr->code - '0' + 52;

				if(chr->code == '+')
					return 62;

				if(chr->code == '/')
					return 63;

				if(chr->code == '=')
					return 64;

				EL_THROW(TException, TString::Format("invalid base64 input %q", *chr));
			}

			// arr_input must contain 4 bytes with values between 0-65, the value 64 is used to indicate padding, 65 indicates EOF
			// arr_output must be able to receive 3 bytes
			// returns the number of bytes decoded, which can be less than 3 at the end of the stream
			static u8_t DecodeBlock(const byte_t* const arr_input, byte_t* const arr_output) EL_WARN_UNUSED_RESULT
			{
				if(arr_input[0] >= 64)
					return 0;

				EL_ERROR(arr_input[1] >= 64, TException, TString("incomplete/invalid base64 sequence"));

				arr_output[0] =   (arr_input[0] & 0b111111) << 2
								| (arr_input[1] & 0b110000) >> 4;

				arr_output[1] =   (arr_input[1] & 0b001111) << 4
								| (arr_input[2] & 0b111100) >> 2;

				arr_output[2] =   (arr_input[2] & 0b000011) << 6
								| (arr_input[3] & 0b111111);

				if(arr_input[2] >= 64)
					return 1;

				if(arr_input[3] >= 64)
					return 2;

				return 3;
			}

			template<typename TSourceStream>
			byte_t* NextItem(TSourceStream* const source)
			{
				if(n_remaining == 0)
				{
					const byte_t v[4] = {
						ReadNextChar(source),
						ReadNextChar(source),
						ReadNextChar(source),
						ReadNextChar(source)
					};

					n_remaining = DecodeBlock(v, buffer);
					index = 0;

					if(n_remaining == 0)
						return nullptr;
				}
				else
				{
					index++;
				}

				n_remaining--;
				return buffer + index;
			}

			TBase64Decoder() : n_remaining(0), index(3) {}
	};

	// has actually 65 symbols - index 64 is translated to '='
	extern const array_t<const TUTF32> BASE64_SYMBOLS;

	class TBase64Encoder
	{
		protected:
			byte_t index : 4;
			byte_t n_remaining : 4;
			byte_t buffer[3];
			TUTF32 chr;

		public:
			using TIn = byte_t;
			using TOut = TUTF32;

			template<typename TSourceStream>
			static unsigned Read(TSourceStream* const source, byte_t* buffer)
			{
				for(unsigned i = 0; i < 3; i++)
				{
					const byte_t* b = source->NextItem();
					if(b == nullptr)
						return i;

					buffer[i] = *b;
				}

				return 3;
			}

			// buffer must always at least 4 bytes large - 3 input bytes will be translated into 4 output bytes
			// if less than 3 input bytes shall be encoded then the rest of the buffer must be filled with zero bytes
			// the function will mark the padding with value 64 which should translate to '='
			static void EncodeBlock(byte_t* buffer, const unsigned n)
			{
				buffer[3] = (buffer[2] & 0b00111111) >> 0;
				buffer[2] = ((buffer[2] & 0b11000000) >> 6) | ((buffer[1] & 0b00001111) << 2);
				buffer[1] = ((buffer[1] & 0b11110000) >> 4) | ((buffer[0] & 0b00000011) << 4);
				buffer[0] = (buffer[0] & 0b11111100) >> 2;

				switch(n)
				{
					case 3:
						break;
					case 2:
						buffer[3] = 64;
						break;
					case 1:
						buffer[2] = 64;
						buffer[3] = 64;
						break;
					case 0:
						buffer[0] = 64;
						buffer[1] = 64;
						buffer[2] = 64;
						buffer[3] = 64;
						break;
				}
			}

			template<typename TSourceStream>
			TUTF32* NextItem(TSourceStream* const source)
			{
				if(n_remaining == 0)
				{
					byte_t local_buffer[4] = {};
					const unsigned r = this->Read(source, local_buffer);

					if(r == 0)
						return nullptr;

					EncodeBlock(local_buffer, r);

					index = 0;
					n_remaining = 3;
					memcpy(this->buffer, local_buffer + 1, 3);
					chr = BASE64_SYMBOLS[local_buffer[0]];
				}
				else
				{
					chr = BASE64_SYMBOLS[this->buffer[index]];
					index++;
					n_remaining--;
				}

				return &chr;
			}

			TBase64Encoder() : index(0), n_remaining(0) {}
	};
}
