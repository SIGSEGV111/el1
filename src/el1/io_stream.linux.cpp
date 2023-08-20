#include "def.hpp"
#ifdef EL_OS_LINUX

#include "io_stream.hpp"
#include "system_waitable.hpp"
#include "system_handle.hpp"
#include "system_task.hpp"

#include <sys/sendfile.h>
// #include <fcntl.h>

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
			return _Pump<byte_t>(source, sink, n_items_max);
		}
	}
}

#endif
