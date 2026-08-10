#include "io_text.hpp"

namespace el1::io::text
{
	using namespace io::collection::array;

	TStreamTextWriter::TStreamTextWriter(stream::IBinarySink* const sink) : sink(sink)
	{
		EL_ERROR(sink == nullptr, TInvalidArgumentException, "sink", "sink cannot be nullptr");
	}

	void TStreamTextWriter::Append(const char32_t* const data, const usys_t length)
	{
		if(length == 0)
			return;
		auto chars = array_t<const char32_t>::FromUnsafePointer(data, length);
		chars.Pipe().Transform(encoding::utf8::TUTF8Encoder()).ToStream(*sink);
	}

	const byte_t* TStreamTextReader::TByteInput::NextItem()
	{
		return source->BlockingRead(&value, 1) == 1 ? &value : nullptr;
	}

	TStreamTextReader::TStreamTextReader(stream::IBinarySource* const source) : byte_input{source}
	{
		EL_ERROR(source == nullptr, TInvalidArgumentException, "source", "source cannot be nullptr");
	}

	bool TStreamTextReader::Extend(const usys_t offset)
	{
		while(!eof && offset >= buffer.Count())
		{
			char32_t* const chr = decoder.NextItem(&byte_input);
			if(chr == nullptr)
			{
				eof = true;
				break;
			}
			buffer.Append(*chr);
		}
		return offset < buffer.Count();
	}

	bool TStreamTextReader::Peek(const usys_t offset, char32_t& out)
	{
		if(!Extend(offset))
			return false;
		out = buffer[offset];
		return true;
	}

	void TStreamTextReader::Consume(const usys_t count)
	{
		EL_ERROR(count > buffer.Count(), TLogicException);
		buffer.Remove(0, count);
	}
}
