#include "def.hpp"
#ifdef EL_OS_LINUX

#include "io_stream.hpp"
#include "system_waitable.hpp"
#include "system_handle.hpp"
#include "system_task.hpp"

#include <sys/sendfile.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

namespace el1::io::stream
{
	using namespace system::handle;
	using namespace system::waitable;
	using namespace system::task;

	template<>
	iosize_t Pump<byte_t>(ISource<byte_t>& source, ISink<byte_t>& sink, const iosize_t n_items_max, const bool blocking)
	{
		if(source.Handle() != INVALID_HANDLE && sink.Handle() != INVALID_HANDLE)
		{
			// const int f_source = EL_SYSERR(fcntl(source.Handle(), F_GETFL));
			// const int f_sink = EL_SYSERR(fcntl(sink.Handle(), F_GETFL));
			// EL_SYSERR(fcntl(source.Handle(), F_SETFL, f_source & (~O_NONBLOCK)));
			// EL_SYSERR(fcntl(sink.Handle(), F_SETFL, f_sink & (~O_NONBLOCK)));
			// EL_SYSERR(fcntl(source.Handle(), F_SETFL, f_source));
			// EL_SYSERR(fcntl(sink.Handle(), F_SETFL, f_sink));

			if(blocking)
			{
				iosize_t n_pumped = 0;
				while(n_pumped < n_items_max)
				{
					const ssize_t r = sendfile(sink.Handle(), source.Handle(), nullptr, n_items_max);
					if(r < 0)
					{
						EL_ERROR(errno != EAGAIN && errno != EWOULDBLOCK, TSyscallException, errno);
						const IWaitable* w_in = source.OnInputReady();
						const IWaitable* w_out = sink.OnOutputReady();

						if(w_in && w_out)
							TFiber::WaitForMany({ w_in, w_out });
						else
							break;
					}

					n_pumped += r;
				}
				return n_pumped;
			}
			else
			{
				return EL_SYSERR(sendfile(sink.Handle(), source.Handle(), nullptr, n_items_max));
			}
		}
		else
		{
			return _Pump<byte_t>(source, sink, n_items_max, blocking);
		}
	}

	/*********************************************/

	usys_t TKernelStream::Read(byte_t* const arr_items, const usys_t n_items_max)
	{
		const ssize_t r = ::read(this->handle, arr_items, n_items_max);
		if(r == -1)
		{
			if(errno == EAGAIN || errno == EWOULDBLOCK)
				return 0;
			EL_THROW(TSyscallException, errno);
		}

		if(r == 0 && n_items_max > 0)
			// EOF
			this->CloseInput();

		return r;
	}

	const system::waitable::THandleWaitable* TKernelStream::OnInputReady() const
	{
		return this->w_input.Handle() >= 0 ? &this->w_input : nullptr;
	}

	bool TKernelStream::CloseInput()
	{
		if(this->w_input.Handle() != -1)
		{
			this->w_input.Handle(-1);
		}

		return true;
	}

	usys_t TKernelStream::Write(const byte_t* const arr_items, const usys_t n_items_max)
	{
		const ssize_t r = ::write(this->handle, arr_items, n_items_max);
		if(r == -1)
		{
			if(errno == EAGAIN || errno == EWOULDBLOCK)
				return 0;
			EL_THROW(TSyscallException, errno);
		}

		return r;
	}

	const system::waitable::THandleWaitable* TKernelStream::OnOutputReady() const
	{
		return this->w_output.Handle() >= 0 ? &this->w_output : nullptr;
	}

	bool TKernelStream::CloseOutput()
	{
		if(this->w_output.Handle() != -1)
		{
			this->w_output.Handle(-1);
		}

		return true;
	}

	void TKernelStream::Flush()
	{
		EL_SYSERR(fdatasync(this->handle));
	}

	TKernelStream::TKernelStream(THandle handle) : handle(std::move(handle)), w_input({.read = true, .write = false, .other = false}, this->handle), w_output({.read = false, .write = true, .other = false}, this->handle)
	{
		const int flags = EL_SYSERR(fcntl(this->handle, F_GETFL));
		EL_SYSERR(fcntl(this->handle, F_SETFL, flags | O_NONBLOCK));
	}

	TKernelStream::TKernelStream(const file::TPath& path) : handle(EL_ANNOTATE_ERROR(EL_SYSERR(open(path, O_RDWR|O_CLOEXEC|O_NOCTTY|O_NONBLOCK)), TException, TString::Format("while opening file %q", path.ToString())), true), w_input({.read = true, .write = false, .other = false}, this->handle), w_output({.read = false, .write = true, .other = false}, this->handle)
	{
	}
}

#endif
