#include "def.hpp"
#ifdef EL_OS_LINUX

#include "system_task.hpp"
#include "io_collection_list.hpp"
#include "io_file.hpp"
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <poll.h>
#include <errno.h>

namespace el1::system::task
{
	using namespace io::collection::list;
	using namespace io::file;

	static EL_THREADLOCAL TThread* thread_self = nullptr;
	static THandle fd_signal;

	TThread* TThread::Self()
	{
		return thread_self;
	}

	struct TMainThread : TThread
	{
		TMainThread()
		{
			thread_self = this;

			sigset_t ss;
			sigfillset(&ss);
			EL_SYSERR(sigdelset(&ss, SIGILL));
			EL_SYSERR(sigdelset(&ss, SIGTRAP));
			EL_SYSERR(sigdelset(&ss, SIGABRT));
			EL_SYSERR(sigdelset(&ss, SIGBUS));
			EL_SYSERR(sigdelset(&ss, SIGFPE));
			EL_SYSERR(sigdelset(&ss, SIGSEGV));
			EL_SYSERR(sigdelset(&ss, SIGCHLD));
			fd_signal = EL_SYSERR(signalfd(-1, &ss, SFD_CLOEXEC|SFD_NONBLOCK));
			EL_SYSERR(sigprocmask(SIG_SETMASK, &ss, nullptr));
			EL_PTHREAD_ERROR(pthread_sigmask(SIG_SETMASK, &ss, nullptr));
		}
	};

	static TMainThread thread_main;

	TThread& TThread::MainThread()
	{
		return thread_main;
	}

	// main thread self-constructor
	TThread::TThread() : name("main"), mutex(), on_state_change(&this->mutex), thread_handle(new pthread_t(pthread_self())), constructor_pid(gettid()), thread_pid(gettid()), starter_pid(getppid()), terminator_pid(-1), state(EChildState::ALIVE), main_fiber(this)
	{
		this->active_fiber = &this->main_fiber;
		this->previous_fiber = &this->main_fiber;
		this->main_fiber.exception = nullptr;
		this->main_fiber.state = EFiberState::ACTIVE;
		this->fibers.Append(&this->main_fiber);
	}

	void* TThread::PthreadMain(void* arg)
	{
		TThread* const myself = reinterpret_cast<TThread*>(arg);
		thread_self = myself;
		myself->thread_pid = gettid();

		try
		{
			try
			{
				myself->main_fiber.main_func();
				const TMutexAutoLock lock(&myself->mutex);
				myself->state = EChildState::FINISHED;
				myself->on_state_change.Raise();
				return nullptr;
			}
			catch(const IException& e)
			{
				myself->main_fiber.exception = std::unique_ptr<const IException>(e.Clone());
			}
			catch(const IException* e)
			{
				myself->main_fiber.exception = std::unique_ptr<const IException>(e);
			}
			catch(shutdown_t)
			{
				const TMutexAutoLock lock(&myself->mutex);
				myself->state = EChildState::FINISHED;
				myself->on_state_change.Raise();
				return nullptr;
			}
			catch(...)
			{
				myself->main_fiber.exception = std::unique_ptr<const IException>(new TUnknownException());
			}

			const TMutexAutoLock lock(&myself->mutex);
			myself->state = EChildState::KILLED;
			myself->on_state_change.Raise();
			return nullptr;
		}
		catch(...)
		{
			return (void*)1;
		}
	}

	/////////////////////////////////////////////////////////////

	void TFiber::KernelWaitForMany(const TList<TFiber*>& fibers)
	{
		TList<struct ::pollfd> pfds;
		TList<const THandleWaitable*> handle_waitables;

		for(TFiber* fiber : fibers)
		{
			if(fiber->state == EFiberState::BLOCKED)
			{
				for(const IWaitable* waitable : fiber->blocked_by)
				{
					const THandleWaitable* const handle_waitable = waitable->HandleWaitable();
					if(handle_waitable != nullptr)
					{
						const THandleWaitable::wait_t wait = handle_waitable->Wait();

						pfds.Append({
							.fd = handle_waitable->Handle(),
							.events = (short)((wait.read ? (POLLIN | POLLRDHUP) : 0) | (wait.write ? POLLOUT : 0) | (wait.other ? POLLPRI : 0)),
							.revents = 0
						});

						handle_waitables.Append(handle_waitable);
					}
				}
			}
		}

		pfds.Append({
			.fd = fd_signal,
			.events = POLLIN,
			.revents = 0
		});

		for(;;)
		{
			const int r = EL_SYSERR(poll(&pfds[0], pfds.Count(), -1));
			if(r > 0)
				break;
			usleep(1000);
		}

		for(unsigned i = 0; i < handle_waitables.Count(); i++)
		{
			handle_waitables[i]->is_ready = pfds[i].revents != 0;
		}

		if(pfds[-1].revents)
		{
			signalfd_siginfo buffer[4];

			for(;;)
			{
				const ssize_t r = read(fd_signal, buffer, sizeof(buffer));
				if(r < 0)
				{
					if(errno == EAGAIN || errno == EWOULDBLOCK)
						break;
					else
						EL_THROW(TSyscallException, errno);
				}
				else
				{
					// process signals
					const unsigned n = r / sizeof(buffer[0]);
					for(unsigned i = 0; i < n; i++)
					{
						switch(buffer[i].ssi_signo)
						{
							case SIGTERM:
							case SIGINT:
							case SIGQUIT:
							case SIGHUP:
								TThread::Self()->MainFiber().Shutdown();
								break;

							default:
								// ignore (TODO: should we care?)
								break;
						}
					}
				}
			}
		}
	}

	/////////////////////////////////////////////////////////////

	TList<fd_t> EnumOpenFileDescriptors()
	{
		TList<fd_t> list;
		TDirectory fd_dir("/proc/self/fd");
		fd_dir.Enum([&](const direntry_t& dent) {
			const fd_t fd = dent.name.ToInteger();
			if(fd != fd_dir.Handle())
				list.Append(fd);
			return true;
		});
		return list;
	}

	/////////////////////////////////////////////////////////////

	TPipe::TPipe() :
		on_input_ready({.read = true, .write = false, .other = false}),
		on_output_ready({.read = false, .write = true, .other = false})
	{
		fd_t fds[2] = {-1,-1};
		EL_SYSERR(pipe2(fds, O_CLOEXEC|O_NONBLOCK));
		rx = fds[0];
		tx = fds[1];

		on_input_ready.Handle(rx);
		on_output_ready.Handle(tx);
	}

}

#endif
