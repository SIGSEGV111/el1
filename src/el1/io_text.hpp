#pragma once

#include "error.hpp"
#include "util.hpp"
#include "io_types.hpp"
#include "io_stream.hpp"
#include "io_collection_list.hpp"
#include "io_text_encoding.hpp"
#include "io_text_string.hpp"

namespace el1::io::text
{
	using namespace io::types;

	struct ITextWriter
	{
		virtual ITextWriter& operator<<(const string::TNumberFormatter* const) = 0;
		virtual ITextWriter& operator<<(const s8_t value) = 0;
		virtual ITextWriter& operator<<(const u8_t value) = 0;
		virtual ITextWriter& operator<<(const s16_t value) = 0;
		virtual ITextWriter& operator<<(const u16_t value) = 0;
		virtual ITextWriter& operator<<(const s32_t value) = 0;
		virtual ITextWriter& operator<<(const u32_t value) = 0;
		virtual ITextWriter& operator<<(const s64_t value) = 0;
		virtual ITextWriter& operator<<(const u64_t value) = 0;
		virtual ITextWriter& operator<<(const double value) = 0;
		virtual ITextWriter& operator<<(const string::TString&) = 0;
		virtual ITextWriter& operator<<(const char* const) = 0;

		inline ITextWriter& Print(const string::TString& s) { return (*this)<<s; }
		inline ITextWriter& Print(const char* const s)      { return (*this)<<s; }

		template<typename ... A>
		ITextWriter& Print(const string::TString& format, A&& ... a)
		{
			return (*this)<<string::TString::Format(format, a...);
		}
	};

	struct TStreamTextWriter : ITextWriter
	{
		stream::IBinarySink* const sink;
		const string::TNumberFormatter* nf;

		ITextWriter& operator<<(const string::TNumberFormatter* const new_nf) final override;
		ITextWriter& operator<<(const s8_t value) final override;
		ITextWriter& operator<<(const u8_t value) final override;
		ITextWriter& operator<<(const s16_t value) final override;
		ITextWriter& operator<<(const u16_t value) final override;
		ITextWriter& operator<<(const s32_t value) final override;
		ITextWriter& operator<<(const u32_t value) final override;
		ITextWriter& operator<<(const s64_t value) final override;
		ITextWriter& operator<<(const u64_t value) final override;
		ITextWriter& operator<<(const double value) final override;
		ITextWriter& operator<<(const string::TString& s) final override;
		ITextWriter& operator<<(const char* const s) final override;
		TStreamTextWriter(stream::IBinarySink* const sink);
	};
}
