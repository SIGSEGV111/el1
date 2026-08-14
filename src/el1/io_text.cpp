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

	bool TStreamTextReader::DoEnsure(const usys_t count)
	{
		if(count <= Count())
			return true;
		if(pos != 0)
		{
			buffer.Remove(0, pos);
			pos = 0;
		}

		while(!eof && buffer.Count() < count)
		{
			char32_t* const chr = decoder.NextItem(&byte_input);
			if(chr == nullptr)
			{
				eof = true;
				break;
			}
			buffer.Append(*chr);
		}
		return buffer.Count() >= count;
	}

	const char32_t& TStreamTextReader::operator[](const usys_t index) const
	{
		EL_ERROR(index >= Count(), stream::TStreamDryException);
		return buffer[pos + index];
	}

	array_t<const char32_t> TStreamTextReader::Head() const noexcept
	{
		const usys_t count = Count();
		return array_t<const char32_t>::FromUnsafePointer(count == 0 ? nullptr : buffer.ItemPtr(pos), count);
	}

	void TStreamTextReader::DoShift(const usys_t count)
	{
		EL_ERROR(count > Count(), TLogicException);
		pos += count;
	}
}
