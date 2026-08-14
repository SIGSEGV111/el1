#include "io_text.hpp"

namespace el1::io::text
{
	using namespace io::collection::array;

	TStreamTextWriter::TStreamTextWriter(stream::IBinarySink* const sink, const usys_t buffer_size)
		: sink(sink), buffer_size(buffer_size), buffer(buffer_size)
	{
		EL_ERROR(sink == nullptr, TInvalidArgumentException, "sink", "sink cannot be nullptr");
		EL_ERROR(buffer_size == 0, TInvalidArgumentException, "buffer_size", "buffer size must be greater than zero");
		buffer.SetCount(buffer_size);
	}

	TStreamTextWriter::~TStreamTextWriter()
	{
		// Destructors cannot report output errors. Explicit Flush() remains the way to
		// observe them; this best-effort flush only prevents normally unwritten tails.
		try { FlushBuffer(); }
		catch(...) {}
	}

	void TStreamTextWriter::FlushBuffer()
	{
		if(n_buffered == 0)
			return;
		sink->WriteAll(buffer.ItemPtr(0), n_buffered);
		n_buffered = 0;
	}

	void TStreamTextWriter::Append(const char32_t* const data, const usys_t length)
	{
		if(length == 0)
			return;

		TCharInput input{data, length, 0};
		for(;;)
		{
			const byte_t* const byte = encoder.NextItem(&input);
			if(byte == nullptr)
				break;

			buffer[n_buffered++] = *byte;
			if(n_buffered == buffer_size)
				FlushBuffer();
		}
	}

	void TStreamTextWriter::Flush()
	{
		FlushBuffer();
		sink->Flush();
	}

	TStreamTextReader::TByteInput::TByteInput(stream::IBinarySource* const source, const usys_t buffer_size)
		: source(source), buffer(buffer_size)
	{
		buffer.SetCount(buffer_size);
	}

	const byte_t* TStreamTextReader::TByteInput::NextItem()
	{
		if(pos == count)
		{
			pos = 0;
			count = source->Read(buffer.ItemPtr(0), buffer.Count());

			// Read() is intentionally attempted first: files and memory sources can fill
			// the complete cache in one call, while sockets/pipes still block for only one
			// byte when no data is currently available.
			if(count == 0)
			{
				count = source->BlockingRead(buffer.ItemPtr(0), 1);
				if(count == 0)
					return nullptr;

				if(buffer.Count() > 1)
					count += source->Read(buffer.ItemPtr(1), buffer.Count() - 1);
			}
		}

		return buffer.ItemPtr(pos++);
	}

	TStreamTextReader::TStreamTextReader(stream::IBinarySource* const source, const usys_t buffer_size)
		: buffer_size(buffer_size),
		  decode_ahead(util::Max<usys_t>(1U, buffer_size / sizeof(char32_t))),
		  byte_input(source, buffer_size),
		  buffer(decode_ahead)
	{
		EL_ERROR(source == nullptr, TInvalidArgumentException, "source", "source cannot be nullptr");
		EL_ERROR(buffer_size == 0, TInvalidArgumentException, "buffer_size", "buffer size must be greater than zero");
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

		const usys_t target = buffer.Count() > NEG1 - decode_ahead
			? count
			: util::Max(count, buffer.Count() + decode_ahead);
		while(!eof && buffer.Count() < target)
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
