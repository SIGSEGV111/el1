#pragma once

#include "error.hpp"
#include "io_types.hpp"
#include "io_stream.hpp"
#include "io_text_encoding.hpp"
#include <string.h>

namespace el1::io::text::string
{
	class TString;
}

namespace el1::io::text::encoding::utf8
{
	using namespace types;
	using namespace text::encoding;

	enum class ESequenceType : u8_t
	{
		NONE = 0,
		SINGLE_BYTE = 1,
		START_BYTE_2 = 2,
		START_BYTE_3 = 3,
		START_BYTE_4 = 4,
		FOLLOW_BYTE = 5
	};

	ESequenceType IdentifyByteType(const byte_t b) EL_GETTER;
	const char* SequenceTypeMnemonic(const ESequenceType type) EL_GETTER;
	u8_t GetEncodedSequenceLength(const TUTF32 chr) EL_GETTER;

	struct TInvalidUtf8SequenceException : error::IException
	{
		iosize_t index;					// index of the first byte of the buffer[] into the overall processed data stream / the number of bytes that have already been processed before
		EDirection dir;
		u8_t	n_bytes_buffer : 3,		// number of bytes in buffer[]
				n_bytes_processed : 2,	// number of bytes in buffer[] successfully processed / position of the error byte in the buffer[]
				seqtype_current : 3;	// the active sequence type; ESequenceType; only values NONE to START_BYTE_4, never FOLLOW_BYTE

		byte_t buffer[6];

		string::TString DumpBuffer() const;
		string::TString Message() const final override;
		error::IException* Clone() const override;

		TInvalidUtf8SequenceException(const iosize_t index, const EDirection dir, const usys_t n_bytes_buffer, const usys_t n_bytes_processed, const ESequenceType seqtype_current, const byte_t* const buffer);
	};

	class TUTF8Decoder
	{
		protected:
			u32_t index;
			TUTF32 buffer;

			template<typename TSourceStream>
			byte_t NextByte(TSourceStream* const source)
			{
				const byte_t* const item = source->NextItem();
				EL_ERROR(item == nullptr, stream::TStreamDryException);
				index++;
				return *item;
			}

		public:
			using TIn = byte_t;
			using TOut = TUTF32;

			template<typename TSourceStream>
			TUTF32* NextItem(TSourceStream* const source)
			{
				const byte_t* const start_byte = source->NextItem();
				if(start_byte == nullptr)
					return nullptr;

				const ESequenceType type = IdentifyByteType(*start_byte);
				unsigned n_need = 0;

				switch(type)
				{
					case ESequenceType::SINGLE_BYTE:
						buffer.code = *start_byte;
						return &buffer;

					case ESequenceType::START_BYTE_2:
						buffer.code = *start_byte & 0b00111111;
						n_need = 1;
						break;

					case ESequenceType::START_BYTE_3:
						buffer.code = *start_byte & 0b00011111;
						n_need = 2;
						break;

					case ESequenceType::START_BYTE_4:
						buffer.code = *start_byte & 0b00001111;
						n_need = 3;
						break;

					case ESequenceType::FOLLOW_BYTE:
						EL_THROW(TInvalidUtf8SequenceException, index, EDirection::DECODING, 4, 0, type, (const byte_t*)&buffer.code);

					// LCOV_EXCL_START
					case ESequenceType::NONE:
						EL_THROW(TLogicException);
					// LCOV_EXCL_STOP
				}

				for(unsigned i = 0; i < n_need; i++)
				{
					const byte_t next = NextByte(source);
					EL_ERROR(IdentifyByteType(next) != ESequenceType::FOLLOW_BYTE, TInvalidUtf8SequenceException, index, EDirection::DECODING, 4, 1+i, type, (const byte_t*)&buffer.code);
					buffer.code <<= 6;
					buffer.code |= (next & 0b00111111);
				}

				return &buffer;
			}

			TUTF8Decoder() : index((u32_t)-1), buffer(0U) {}
	};

	class TUTF8Encoder
	{
		protected:
			u32_t index;

			union
			{
				byte_t buffer[4];
				u32_t buffer_value;
			};

		public:
			using TIn = TUTF32;
			using TOut = byte_t;

			template<typename TSourceStream>
			byte_t* NextItem(TSourceStream* const source)
			{
				buffer_value >>= 8;

				if(buffer[0] == 0)
				{
					const TUTF32* const item = source->NextItem();
					if(item == nullptr)
						return nullptr;

					index++;

					if(item->code <= 127)
					{
						buffer[0] = item->code;
					}
					else if(item->code < 2048)
					{
						buffer[0] = 0b11000000 | ((item->code >>  6) & 0b00011111);
						buffer[1] = 0b10000000 | ((item->code >>  0) & 0b00111111);
					}
					else if(item->code < 65536)
					{
						buffer[0] = 0b11100000 | ((item->code >> 12) & 0b00001111);
						buffer[1] = 0b10000000 | ((item->code >>  6) & 0b00111111);
						buffer[2] = 0b10000000 | ((item->code >>  0) & 0b00111111);
					}
					else if(item->code < 4194304)
					{
						buffer[0] = 0b11110000 | ((item->code >> 18) & 0b00001111);
						buffer[1] = 0b10000000 | ((item->code >> 12) & 0b00111111);
						buffer[2] = 0b10000000 | ((item->code >>  6) & 0b00111111);
						buffer[3] = 0b10000000 | ((item->code >>  0) & 0b00111111);
					}
					else
						EL_THROW(TInvalidUtf8SequenceException, index, EDirection::ENCODING, 4, 0, ESequenceType::NONE, (const byte_t*)item);
				}

				return buffer + 0;
			}

			TUTF8Encoder() : index((u32_t)-1), buffer_value(0)
			{
			}
	};
}

#ifdef EL_CHAR_IS_UTF8
namespace el1::io::text::encoding
{
	using TCharEncoder = utf8::TUTF8Encoder;
	using TCharDecoder = utf8::TUTF8Decoder;
}
#endif
