#include "def.hpp"
#ifdef EL_OS_CLASS_POSIX

#include "system_task.hpp"
#include "io_collection_list.hpp"
#include "io_collection_map.hpp"
#include <unistd.h>
#include <pthread.h>
#include <malloc.h>
#include <signal.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <poll.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>

extern char** environ;

namespace el1::system::task
{
	using namespace error;
	using namespace io::collection::list;
	using namespace io::collection::map;
	using namespace io::text::string;
	using namespace io::stream;
	using namespace io::file;

	/***************************************************/

	bool TSimpleMutex::Accquire(const TTime timeout)
	{
		if(this->IsAcquired())
		{
			this->n_accquire++;
			return true;
		}
		else if(timeout >= 0)
		{
			if(timeout > 0)
			{
				const TTime end = TTime::Now() + timeout;

				while(TTime::Now() < end)
				{
					const int code = pthread_mutex_trylock(reinterpret_cast<pthread_mutex_t*>(this->data));
					if(code == 0)
					{
						this->owner = TThread::Self();
						this->n_accquire = 1U;
						return true;
					}
					else if(code == EBUSY)
					{
						sched_yield();
					}
					else
					{
						EL_THROW(TPthreadException, code);
					}
				}
			}

			const int code = pthread_mutex_trylock(reinterpret_cast<pthread_mutex_t*>(this->data));
			if(code == 0)
			{
				this->owner = TThread::Self();
				this->n_accquire = 1;
				return true;
			}
			else if(code == EBUSY)
			{
				return false;
			}
			else
			{
				EL_THROW(TPthreadException, code);
			}
		}
		else
		{
			EL_PTHREAD_ERROR(pthread_mutex_lock(reinterpret_cast<pthread_mutex_t*>(this->data)));
			this->owner = TThread::Self();
			this->n_accquire = 1U;
			return true;
		}
	}

	void TSimpleMutex::Release()
	{
		this->n_accquire--;
		if(this->n_accquire == 0U)
		{
			this->owner = nullptr;
			EL_PTHREAD_ERROR(pthread_mutex_unlock(reinterpret_cast<pthread_mutex_t*>(this->data)));
		}
	}

	bool TSimpleMutex::IsAcquired() const
	{
		return this->owner == TThread::Self() && this->n_accquire > 0U;
	}

	TSimpleMutex::TSimpleMutex() : data(new pthread_mutex_t()), owner(nullptr), n_accquire(0U)
	{
		pthread_mutexattr_t attr;

		try
		{
			EL_PTHREAD_ERROR(pthread_mutexattr_init(&attr));

			try
			{
				EL_PTHREAD_ERROR(pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST));
				// EL_PTHREAD_ERROR(pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE));
				EL_PTHREAD_ERROR(pthread_mutex_init(reinterpret_cast<pthread_mutex_t*>(this->data), &attr));
			}
			catch(...)
			{
				pthread_mutexattr_destroy(&attr);
				throw;
			}
		}
		catch(...)
		{
			delete reinterpret_cast<pthread_mutex_t*>(this->data);
			throw;
		}
	}

	TSimpleMutex::~TSimpleMutex()
	{
		pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t*>(this->data));
		delete reinterpret_cast<pthread_mutex_t*>(this->data);
	}

	/***************************************************/

	bool TSimpleSignal::WaitFor(const TTime timeout) const
	{
		if(timeout >= 0)
		{
			const timespec ts = timeout;
			const int code = pthread_cond_timedwait(reinterpret_cast<pthread_cond_t*>(this->data), reinterpret_cast<pthread_mutex_t*>(this->mutex->data), &ts);
			if(code == 0)
				return true;
			else if(code == ETIMEDOUT)
				return false;
			else
				EL_THROW(TPthreadException, code);
		}
		else
		{
			EL_PTHREAD_ERROR(pthread_cond_wait(reinterpret_cast<pthread_cond_t*>(this->data), reinterpret_cast<pthread_mutex_t*>(this->mutex->data)));
			return true;
		}
	}

	void TSimpleSignal::Raise()
	{
		EL_PTHREAD_ERROR(pthread_cond_broadcast(reinterpret_cast<pthread_cond_t*>(this->data)));
	}

	TSimpleSignal::TSimpleSignal(TSimpleMutex* const mutex) : mutex(mutex), data(new pthread_cond_t(PTHREAD_COND_INITIALIZER))
	{
	}

	TSimpleSignal::~TSimpleSignal()
	{
		pthread_cond_destroy(reinterpret_cast<pthread_cond_t*>(this->data));
		delete reinterpret_cast<pthread_cond_t*>(this->data);
	}

	/***************************************************/

	ETaskState TThread::TaskState() const
	{
		EL_NOT_IMPLEMENTED;
	}

	void TThread::Start()
	{
		const TMutexAutoLock lock(&this->mutex);
		EL_ERROR(this->state != EChildState::CONSTRUCTED, TInvalidChildStateException, false, EChildState::CONSTRUCTED, this->state);

		if(this->thread_handle == nullptr)
			this->thread_handle = new pthread_t();

		EL_PTHREAD_ERROR(pthread_create(reinterpret_cast<pthread_t*>(this->thread_handle), nullptr, &TThread::PthreadMain, this));
		this->state = EChildState::ALIVE;
		this->on_state_change.Raise();
		this->starter_pid = gettid();
	}

	void TThread::Stop()
	{
		const TMutexAutoLock lock(&this->mutex);
		EL_ERROR(this->state != EChildState::ALIVE, TInvalidChildStateException, false, EChildState::ALIVE, this->state);
		EL_PTHREAD_ERROR(pthread_kill(*reinterpret_cast<pthread_t*>(this->thread_handle), SIGSTOP));
	}

	void TThread::Resume()
	{
		const TMutexAutoLock lock(&this->mutex);
		EL_ERROR(this->state != EChildState::ALIVE, TInvalidChildStateException, false, EChildState::ALIVE, this->state);
		EL_PTHREAD_ERROR(pthread_kill(*reinterpret_cast<pthread_t*>(this->thread_handle), SIGCONT));
	}

	bool TThread::Kill()
	{
		const TMutexAutoLock lock(&this->mutex);
		if(this->state == EChildState::ALIVE)
		{
			EL_PTHREAD_ERROR(pthread_kill(*reinterpret_cast<pthread_t*>(this->thread_handle), SIGKILL));
			this->state = EChildState::KILLED;
			this->on_state_change.Raise();
			this->terminator_pid = gettid();
			return true;
		}
		else
			return false;
	}

	bool TThread::Shutdown()
	{
		const TMutexAutoLock lock(&this->mutex);
		if(this->state == EChildState::ALIVE)
		{
			EL_PTHREAD_ERROR(pthread_kill(*reinterpret_cast<pthread_t*>(this->thread_handle), SIGTERM));
			return true;
		}
		else
			return false;
	}

	std::unique_ptr<const IException> TThread::Join()
	{
		TMutexAutoLock lock_data(&this->mutex);
		EL_ERROR(this->state == EChildState::CONSTRUCTED, TInvalidChildStateException, true, EChildState::CONSTRUCTED, this->state);
		EL_ERROR(this->state == EChildState::JOINING, TInvalidChildStateException, true, EChildState::CONSTRUCTED, this->state);

		this->joiner_pid = gettid();
		this->state = EChildState::JOINING;
		this->on_state_change.Raise();

		void* exit_code = nullptr;
		lock_data.mutex->Release();
		EL_PTHREAD_ERROR(pthread_join(*reinterpret_cast<pthread_t*>(this->thread_handle), &exit_code));
		lock_data.mutex->Accquire();

		this->state = EChildState::CONSTRUCTED;
		this->on_state_change.Raise();

		if(exit_code != nullptr)
			return std::unique_ptr<const IException>(new TUnknownException);

		return std::move(this->main_fiber.exception);
	}

	TThread::~TThread()
	{
		// the main-thread has (thread_pid == constructor_pid) and must not be handled here
		if(this != &TThread::MainThread())
		{
			if(this->state != EChildState::CONSTRUCTED)
			{
				try
				{
					this->Shutdown();
					std::unique_ptr<const IException> e = this->Join();
					if(e)
					{
						e->Print("thread terminated with exception");
					}
				}
				catch(const IException& e)
				{
					e.Print("exception during thread shutdown");
				}
			}

			delete reinterpret_cast<pthread_t*>(this->thread_handle);
		}
	}

	static TSortedMap<TString, TString> ReadEnvironment()
	{
		TSortedMap<TString, TString> map;

		if(::environ == nullptr)
			return map;

		for(char** p = ::environ; *p != nullptr; p++)
		{
			const char* const p_equal = strchr(*p, '=');
			EL_ERROR(p_equal == nullptr, TLogicException);
			TString key(*p, p_equal - *p);
			TString value(p_equal + 1);
			map.Add(std::move(key), std::move(value));
		}

		return map;
	}

	const TSortedMap<TString, const TString>& EnvironmentVariables()
	{
		static TSortedMap<TString, TString> env = ReadEnvironment();
		return env;
	}

	/////////////////////////////////////////////////////////////

	usys_t TPipe::Read(byte_t* const arr_items, const usys_t n_items_max)
	{
		if(rx == -1)
			return 0;

		const ssize_t n = read(rx, arr_items, n_items_max);
		if(n < 0)
		{
			if(errno == EAGAIN || errno == EWOULDBLOCK)
			{
				return 0;
			}
			else
			{
				EL_THROW(TSyscallException, errno);
			}
		}
		else if(n == 0)
		{
			rx.Close();
			on_input_ready.Handle(-1);
			return 0;
		}
		else
		{
			return n;
		}
	}

	usys_t TPipe::Write(const byte_t* const arr_items, const usys_t n_items_max)
	{
		if(tx == -1)
			return 0;

		const ssize_t n = write(tx, arr_items, n_items_max);
		if(n < 0)
		{
			if(errno == EAGAIN || errno == EWOULDBLOCK)
			{
				return 0;
			}
			else
			{
				EL_THROW(TSyscallException, errno);
			}
		}
		else if(n == 0)
		{
			tx.Close();
			on_output_ready.Handle(-1);
			return 0;
		}
		else
		{
			return n;
		}
	}

	THandleWaitable* TPipe::OnInputReady() const
	{
		return rx == -1 ? nullptr : &on_input_ready;
	}

	THandleWaitable* TPipe::OnOutputReady() const
	{
		return tx == -1 ? nullptr : &on_output_ready;
	}

	void TPipe::Close()
	{
		rx.Close();
		tx.Close();
	}

	/////////////////////////////////////////////////////////////

	static const unsigned PIDFD_NONBLOCK = O_NONBLOCK;

	extern "C" int pidfd_open(pid_t pid, unsigned int flags)
	{
		return syscall(SYS_pidfd_open, pid, flags);
	}

	void TProcess::Start(const TPath& exe, const TList<TString>& args, const TSortedMap<fd_t, const EFDIO>& streams, const TSortedMap<TString, const TString>& env)
	{
		EL_ERROR(pid != -1, TException, "process already running");

		TSortedMap<fd_t, THandle> child_handles;
		this->streams.Clear();

		for(const auto& kv : streams.Items())
		{
			EL_ERROR(kv.key < 0, TInvalidArgumentException, "streams", "file-descriptor cannot be negative");

			switch(kv.value)
			{
				case EFDIO::INHERIT:
					// TODO: remove O_CLOEXEC
					continue;

				case EFDIO::EMPTY:
				case EFDIO::DISCARD:
				case EFDIO::ZERO:
				case EFDIO::RANDOM:
					continue;

				case EFDIO::PIPE_PARENT_TO_CHILD:
				case EFDIO::PIPE_CHILD_TO_PARENT:
					break;
			}

			TPipe pipe;

			if(kv.value == EFDIO::PIPE_PARENT_TO_CHILD)
			{
				pipe.SendSide().BlockingIO(false);
				child_handles.Add(kv.key, std::move(pipe.ReceiveSide()));
				this->streams.Add(kv.key, std::move(pipe));
			}
			else
			{
				pipe.ReceiveSide().BlockingIO(false);
				child_handles.Add(kv.key, std::move(pipe.SendSide()));
				this->streams.Add(kv.key, std::move(pipe));
			}
		}

		EL_SYSERR(pid = fork());
		if(pid == 0)
		{
			try
			{
				// child

				fd_t fd_it = 0;
				for(const auto& kv : streams.Items())
				{
					switch(kv.value)
					{
						case EFDIO::INHERIT:
						case EFDIO::EMPTY:
						case EFDIO::DISCARD:
						case EFDIO::ZERO:
						case EFDIO::RANDOM:
							continue;

						case EFDIO::PIPE_PARENT_TO_CHILD:
						case EFDIO::PIPE_CHILD_TO_PARENT:
							if(streams.Get(child_handles[kv.key]) != nullptr)
							{
								// find "free" FD
								for(; streams.Get(fd_it) != nullptr || child_handles.Get(fd_it) != nullptr; fd_it++);
								fd_it++;
								EL_SYSERR(dup2(child_handles[kv.key], fd_it));
								child_handles[kv.key] = fd_it;
							}
							else
							{
								// make sure nobody dup2's over our conflict-free FD
								fd_it = util::Max(child_handles[kv.key] + 1, fd_it);
							}
							break;
					}
				}

				for(const auto& kv : streams.Items())
				{
					fd_t fd_tmp = -1;
					switch(kv.value)
					{
						case EFDIO::INHERIT:
							// nothing to do
							break;

						case EFDIO::EMPTY:
							EL_SYSERR(fd_tmp = open("/dev/null", O_RDONLY|O_NOCTTY));
							EL_SYSERR(dup2(fd_tmp, kv.key));
							EL_SYSERR(close(fd_tmp));
							break;

						case EFDIO::DISCARD:
							EL_SYSERR(fd_tmp = open("/dev/null", O_RDWR|O_NOCTTY));
							EL_SYSERR(dup2(fd_tmp, kv.key));
							EL_SYSERR(close(fd_tmp));
							break;

						case EFDIO::ZERO:
							EL_SYSERR(fd_tmp = open("/dev/zero", O_RDONLY|O_NOCTTY));
							EL_SYSERR(dup2(fd_tmp, kv.key));
							EL_SYSERR(close(fd_tmp));
							break;

						case EFDIO::RANDOM:
							EL_SYSERR(fd_tmp = open("/dev/urandom", O_RDONLY|O_NOCTTY));
							EL_SYSERR(dup2(fd_tmp, kv.key));
							EL_SYSERR(close(fd_tmp));
							break;

						case EFDIO::PIPE_PARENT_TO_CHILD:
						case EFDIO::PIPE_CHILD_TO_PARENT:
							EL_SYSERR(dup2(child_handles[kv.key], kv.key));
							child_handles[kv.key].Close();
							break;
					}
				}

				{
					const TList<fd_t> open_fds = EnumOpenFileDescriptors();
					for(const fd_t fd : open_fds)
						if(streams.Get(fd) == nullptr)
							close(fd);
				}

				TList<const char*> args_cstr;
				args_cstr.Append(exe);
				for(const auto& str : args)
					args_cstr.Append(str.MakeCStr().release());
				args_cstr.Append(nullptr);

				TList<const char*> env_cstr;
				for(const auto& kv : env.Items())
				{
					env_cstr.Append((kv.key + "=" + kv.value).MakeCStr().release());
				}
				env_cstr.Append(nullptr);

				{
					sigset_t ss;
					sigemptyset(&ss);
					EL_SYSERR(sigprocmask(SIG_SETMASK, &ss, nullptr));
				}

				EL_SYSERR(execve(exe, (char**)&args_cstr[0], (char**)&env_cstr[0]));
				EL_THROW(TLogicException);
			}
			catch(const IException& e)
			{
				e.Print("error while configuring the child process");
			}
			catch(...)
			{
			}
			_exit(255);
		}
		else
		{
			// parent
			h_proc = EL_SYSERR(pidfd_open(pid, PIDFD_NONBLOCK));
			on_terminate.Handle(h_proc);
		}
	}

	void TProcess::Stop()
	{
		EL_ERROR(pid == -1, TException, "process not started yet");
		kill(pid, SIGSTOP);
	}

	void TProcess::Resume()
	{
		EL_ERROR(pid == -1, TException, "process not started yet");
		kill(pid, SIGCONT);
	}

	bool TProcess::Kill()
	{
		EL_ERROR(pid == -1, TException, "process not started yet");
		return kill(pid, SIGKILL) == 0;
	}

	bool TProcess::Shutdown()
	{
		EL_ERROR(pid == -1, TException, "process not started yet");
		return kill(pid, SIGTERM) == 0;
	}

	int TProcess::Join()
	{
		EL_ERROR(pid == -1, TException, "process not started yet");

		for(;;)
		{
			int wstatus = 0;
			const pid_t wait_pid = EL_SYSERR(waitpid(pid, &wstatus, WNOHANG));

			if(wait_pid == 0)
			{
				on_terminate.WaitFor();
			}
			else
			{
				pid = -1;
				h_proc.Close();
				on_terminate.Handle(-1);

				if(WIFEXITED(wstatus))
				{
					return WEXITSTATUS(wstatus);
				}
				else if(WIFSIGNALED(wstatus))
				{
					return -WTERMSIG(wstatus);
				}
				else
				{
					EL_THROW(TLogicException);
				}
			}
		}
	}

	ETaskState TProcess::TaskState() const
	{
		if(pid == -1)
			return ETaskState::NOT_CREATED;
		else
			return ETaskState::RUNNING; // FIXME
	}
}

#endif
