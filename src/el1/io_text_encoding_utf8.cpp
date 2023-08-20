#include "def.hpp"
#include "io_types.hpp"
#include "io_text_encoding_utf8.hpp"
#include "io_text_string.hpp"
#include "util.hpp"
#include <string.h>

namespace el1::io::text::encoding::utf8
{
	using namespace io::types;
	using namespace io::text::encoding;
	using namespace io::text::string;

	ESequenceType IdentifyByteType(const byte_t byte)
	{
		if( (byte & 0b10000000) == 0b00000000 )
			return ESequenceType::SINGLE_BYTE;

		if( (byte & 0b11000000) == 0b10000000 )
			return ESequenceType::FOLLOW_BYTE;

		if( (byte & 0b11110000) == 0b11110000 )
			return ESequenceType::START_BYTE_4;

		if( (byte & 0b11110000) == 0b11100000 )
			return ESequenceType::START_BYTE_3;

		if( (byte & 0b11100000) == 0b11000000 )
			return ESequenceType::START_BYTE_2;

		EL_THROW(TLogicException);
	}

	const char* SequenceTypeMnemonic(const ESequenceType type)
	{
		switch(type)
		{
			case ESequenceType::NONE:  return "NONE";
			case ESequenceType::SINGLE_BYTE:  return "ASCII";
			case ESequenceType::START_BYTE_2: return "STB2";
			case ESequenceType::START_BYTE_3: return "STB3";
			case ESequenceType::START_BYTE_4: return "STB4";
			case ESequenceType::FOLLOW_BYTE:  return "FOLB";
		}

		EL_THROW(TLogicException);
	}

	u8_t GetEncodedSequenceLength(const TUTF32 chr)
	{
		if(chr.code <= 127)
			return 1;

		if(chr.code < 2048)
			return 2;

		if(chr.code < 65536)
			return 3;

		// actually UTF32 defines 1114111 (0x10FFFF) as the last valid code point, but it is *NOT* our duty to validate that here
		// garbage in => garbage out
		if(chr.code < 4194304)
			return 4;

		EL_THROW(TInvalidUtf8SequenceException, 0, EDirection::ENCODING, 0, 0, ESequenceType::NONE, nullptr);
	}

	/*********************************/

	TInvalidUtf8SequenceException::TInvalidUtf8SequenceException(const iosize_t index, const EDirection dir, const usys_t n_bytes_buffer, const usys_t n_bytes_processed, const ESequenceType seqtype_current, const byte_t* const buffer) : index(index), dir(dir)
	{
		this->n_bytes_buffer = util::Min((usys_t)sizeof(this->buffer), n_bytes_buffer);
		this->n_bytes_processed = n_bytes_processed;
		this->seqtype_current = (u8_t)seqtype_current;

		memset(this->buffer, 0, sizeof(this->buffer));
		memcpy(this->buffer, buffer, this->n_bytes_buffer);
	}

	TString TInvalidUtf8SequenceException::DumpBuffer() const
	{
		TString msg;
		for(u8_t i = 0; i < n_bytes_buffer; i++)
		{
			const byte_t& byte = buffer[i];
			msg += TString::Format(" %02x (%s)", byte, SequenceTypeMnemonic(IdentifyByteType(byte)));
		}
		msg.Trim();
		return msg;
	}

	TString TInvalidUtf8SequenceException::Message() const
	{
		return TString::Format("an invalid UTF8 byte-sequence was encountered at IO-index %d; bytes in buffer: %s", index, DumpBuffer());
	}

	error::IException* TInvalidUtf8SequenceException::Clone() const
	{
		return new TInvalidUtf8SequenceException(*this);
	}
}
