#include "io_text.hpp"

namespace el1::io::text
{
	using namespace io::collection::list;

	ITextWriter& TStreamTextWriter::operator<<(const string::TNumberFormatter* const new_nf)
	{
		EL_ERROR(new_nf == nullptr, TInvalidArgumentException, "new_nf", "new_nf cannot be nullptr");
		this->nf = new_nf;
		return *this;
	}

	ITextWriter& TStreamTextWriter::operator<<(const s8_t value)
	{
		return (*this)<<nf->Format(value);
	}

	ITextWriter& TStreamTextWriter::operator<<(const u8_t value)
	{
		return (*this)<<nf->Format(value);
	}

	ITextWriter& TStreamTextWriter::operator<<(const s16_t value)
	{
		return (*this)<<nf->Format(value);
	}

	ITextWriter& TStreamTextWriter::operator<<(const u16_t value)
	{
		return (*this)<<nf->Format(value);
	}

	ITextWriter& TStreamTextWriter::operator<<(const s32_t value)
	{
		return (*this)<<nf->Format(value);
	}

	ITextWriter& TStreamTextWriter::operator<<(const u32_t value)
	{
		return (*this)<<nf->Format(value);
	}

	ITextWriter& TStreamTextWriter::operator<<(const s64_t value)
	{
		return (*this)<<nf->Format(value);
	}

	ITextWriter& TStreamTextWriter::operator<<(const u64_t value)
	{
		return (*this)<<nf->Format(value);
	}

	ITextWriter& TStreamTextWriter::operator<<(const double value)
	{
		return (*this)<<nf->Format(value);
	}

	ITextWriter& TStreamTextWriter::operator<<(const string::TString& s)
	{
		s.chars.Pipe().Transform(encoding::utf8::TUTF8Encoder()).ToStream(*sink);
		return *this;
	}

	ITextWriter& TStreamTextWriter::operator<<(const char* const s)
	{
		sink->WriteAll((byte_t*)s, strlen(s));
		return *this;
	}

	TStreamTextWriter::TStreamTextWriter(stream::IBinarySink* const sink) : sink(sink), nf(string::TNumberFormatter::DEFAULT_DECIMAL)
	{
		EL_ERROR(sink == nullptr, TInvalidArgumentException, "sink", "sink cannot be nullptr");
		EL_ERROR(nf == nullptr, TInvalidArgumentException, "nf", "nf cannot be nullptr");
	}
}
