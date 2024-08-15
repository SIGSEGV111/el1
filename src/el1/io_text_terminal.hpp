#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include "io_text_string.hpp"
#include "io_stream.hpp"
#include "math.hpp"
#include <utility>

namespace el1::io::text::terminal
{
	using namespace types;

	struct rgba8_t
	{
		union
		{
			u8_t v[4];

			struct
			{
				u8_t red, green, blue, alpha;
			};
		};
	};

	struct ITerminal
	{
		virtual ITerminal& operator<<(const string::TString& str) = 0;
		virtual string::TString TextColorCode(rgba8_t rgb) const = 0;
		virtual string::TString BackgroundColorCode(rgba8_t rgb) const = 0;
		virtual math::vector_t<u16_t, 2> WindowSize() const = 0;

		template<typename ... R>
		void Print(string::TString format, R&& ... r)
		{
			(*this)<<string::TString::Format(std::move(format), std::forward<R>(r) ...);
		}
	};

	class TNoTerminal : public ITerminal
	{
		public:
			ITerminal& operator<<(const string::TString& str) final override;
			string::TString TextColorCode(rgba8_t rgb) const final override;
			string::TString BackgroundColorCode(rgba8_t rgb) const final override;
			math::vector_t<u16_t, 2> WindowSize() const final override;
	};

	// the "terminal" is a human facing text interface
	// the terminal should NOT be used to drive data in a pipline-style program
	// instead the underlying byte-oriented I/O-streams should be used directly (see below)
	// el1 will auto-detect the terminal type and instantiate the correct driver class
	// when run standalone the terminal uses stdin and stderr to communicate
	// if el1 detects that the program is used in a pipeline-style then it will use TNoTerminal
	// and not connect stdin at all - all input operations always return EOF
	extern ITerminal& term;

	extern stream::TKernelStream stdin;
	extern stream::TKernelStream stdout;
	extern stream::TKernelStream stderr;
}
