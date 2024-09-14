#include "def.hpp"
#ifdef EL_OS_CLASS_POSIX

#include "io_text_terminal.hpp"
#include "io_file.hpp"
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

namespace el1::io::text::terminal
{
	static system::handle::THandle OpenFile(const io::text::string::TString& filename, const io::file::TAccess access)
	{
		return system::handle::THandle(EL_SYSERR(open(filename.MakeCStr().get(), O_CLOEXEC | O_NOCTTY | access.FileOpenFlags())), true);
	}

	static system::handle::THandle ReopenStdio(const system::handle::fd_t fd, const io::file::TAccess access)
	{
		try
		{
			// try to open the FD again under a new independant FD ...
			auto handle = OpenFile(io::text::string::TString::Format("/proc/self/fd/%d", fd), access);
			const off_t pos = lseek(fd, SEEK_CUR, 0);
			if(pos > 0)
				EL_SYSERR(lseek(handle, SEEK_SET, pos));
			return handle;
		}
		catch(...)
		{
			// ... return the original handle if it fails
			return system::handle::THandle(fd, true);
		}
	}

	stream::TKernelStream stdin (ReopenStdio(STDIN_FILENO,  io::file::TAccess::RO));
	stream::TKernelStream stdout(ReopenStdio(STDOUT_FILENO, io::file::TAccess::WO));
	stream::TKernelStream stderr(ReopenStdio(STDERR_FILENO, io::file::TAccess::WO));

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

	math::vector::vector_t<u16_t, 2> TNoTerminal::WindowSize() const
	{
		struct winsize size;
		EL_SYSERR(ioctl(1, TIOCGWINSZ, &size));
		return { size.ws_col, size.ws_row };
	}

	static TNoTerminal no_term;
	ITerminal& term = no_term;
}

#endif
