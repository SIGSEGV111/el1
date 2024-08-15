#include "def.hpp"
#ifdef EL_OS_CLASS_POSIX

#include "io_text_terminal.hpp"
#include <sys/ioctl.h>

namespace el1::io::text::terminal
{
	stream::TKernelStream stdin (system::handle::THandle(0, true));
	stream::TKernelStream stdout(system::handle::THandle(1, true));
	stream::TKernelStream stderr(system::handle::THandle(2, true));

	ITerminal& TNoTerminal::operator<<(const string::TString& str)
	{
		str.chars.Pipe().Transform(encoding::utf8::TUTF8Encoder()).ToStream(stdout);
		return *this;
	}

	string::TString TNoTerminal::TextColorCode(rgba8_t rgb) const
	{
		return string::TString();
	}

	string::TString TNoTerminal::BackgroundColorCode(rgba8_t rgb) const
	{
		return string::TString();
	}

	math::vector_t<u16_t, 2> TNoTerminal::WindowSize() const
	{
		struct winsize size;
		EL_SYSERR(ioctl(1, TIOCGWINSZ, &size));
		return { size.ws_col, size.ws_row };
	}

	static TNoTerminal no_term;
	ITerminal& term = no_term;
}

#endif
