#include "system_task.hpp"
#include "system_time_timer.hpp"
#include "system_memory.hpp"
#include "error.hpp"
#include "util.hpp"
#include "def.hpp"
#include "io_text_encoding_utf8.hpp"
#include "io_text_string.hpp"
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#ifdef EL1_WITH_VALGRIND
#include <valgrind/valgrind.h>
#endif

#ifdef __SANITIZE_ADDRESS__
#include <sanitizer/common_interface_defs.h>
#endif

#define IF_DEBUG_PRINTF(...) if(EL_UNLIKELY(DEBUG)) fprintf(stderr, __VA_ARGS__)

namespace el1::system::task
{
	using namespace io::stream;
	using namespace io::file;
	using namespace io::text::string;
	using namespace io::collection::list;
	using namespace io::collection::map;
	using namespace time::timer;
	using namespace memory;

	const char* TInvalidChildStateException::StateMnemonic(const EChildState state)
	{
		switch(state)
		{
			case EChildState::CONSTRUCTED:	return "CONSTRUCTED";
			case EChildState::ALIVE:		return "ALIVE";
			case EChildState::FINISHED:		return "FINISHED";
			case EChildState::KILLED:		return "KILLED";
			case EChildState::JOINING:		return "JOINING";
		}

		EL_THROW(TLogicException);
	}

	TString TInvalidChildStateException::Message() const
	{
		if(this->inverted)
			return TString::Format("child process was in unacceptable state %q", StateMnemonic(this->actual_state));
		else
			return TString::Format("child process was in state %q, but control logic expected state %q", StateMnemonic(this->actual_state), StateMnemonic(this->expected_state));
	}

	error::IException* TInvalidChildStateException::Clone() const
	{
		return new TInvalidChildStateException(*this);
	}

	/***************************************************/

	bool TFiberMutex::Accquire(const TTime timeout)
	{
		TFiber* self = TFiber::Self();

		if(owner != nullptr && owner != self)
		{
			if(timeout == 0)
				return false;

			TTimeWaitable wait_timeout(EClock::MONOTONIC, TTime::Now(EClock::MONOTONIC) + timeout);
			TMemoryWaitable<usys_t> wait_locked((usys_t*)&owner, (usys_t*)NEG1, NEG1);

			while(owner != nullptr)
			{
				if(timeout < 0)
				{
					// infinite timeout
					wait_locked.WaitFor();
				}
				else //if(timeout > 0)
				{
					// use waitable
					TFiber::WaitForMany({&wait_locked, &wait_timeout});
				}
			}
		}

		owner = TFiber::Self();
		n_accquire++;
		return true;
	}

	void TFiberMutex::Release()
	{
		EL_ERROR(owner != TFiber::Self(), TException, "tried to release a TFiberMutex that was not owned by the calling fiber");
		n_accquire--;
		if(n_accquire == 0)
			owner = nullptr;
	}

	bool TFiberMutex::IsAcquired() const
	{
		return TFiber::Self() == owner;
	}

	TFiberMutex::TFiberMutex() : owner(nullptr), n_accquire(0)
	{
	}

	TFiberMutex::~TFiberMutex()
	{
		EL_WARN(owner != nullptr, TException, "TFiberMutex was still locked while beeing destructed");
	}

	/***************************************************/

	TThread::TThread(const TString name, TFunction<void> main_func, const bool autostart) : name(name), mutex(), on_state_change(&this->mutex), thread_handle(nullptr), constructor_pid(TThread::Self()->ThreadPID()), thread_pid(-1), starter_pid(-1), terminator_pid(-1), state(EChildState::CONSTRUCTED), main_fiber(this)
	{
		this->active_fiber = &this->main_fiber;
		this->previous_fiber = &this->main_fiber;
		this->main_fiber.main_func = main_func;
		this->main_fiber.exception = nullptr;
		this->main_fiber.state = EFiberState::ACTIVE;
		this->fibers.Append(&this->main_fiber);
		if(autostart)
			this->Start();
	}

	/***************************************************/

	bool TFiber::DEBUG = false;
	EStackAllocator TFiber::DEFAULT_STACK_ALLOCATOR = EStackAllocator::MALLOC;
	usys_t TFiber::FIBER_DEFAULT_STACK_SIZE_BYTES = 16 * 1024;

	usys_t TFiber::StackFree() const
	{
		volatile const byte_t marker = 0;
		TFiber* const self = TThread::Self()->ActiveFiber();
		const usys_t base = (usys_t)this->p_stack;
		const usys_t sp = this == self ? (usys_t)&marker : (usys_t)this->registers.EL_ARCH_STACKPOINTER_REGISTER_NAME;

		if(sp < base)
		{
			raise(SIGABRT);
			throw "stack pointer underrun!";
		}

		return sp - base;
	}

	void TFiber::WaitForMany(const std::initializer_list<const IWaitable*> waitables)
	{
		WaitForMany(array_t<const IWaitable*>((const IWaitable**)waitables.begin(), waitables.end() - waitables.begin()));
	}

	void TFiber::WaitForMany(const array_t<const IWaitable*> waitables)
	{
		TFiber* self = TThread::Self()->ActiveFiber();
		IF_DEBUG_PRINTF("TFiber@%p::WaitForMany(waitables=%zu): ENTER\n", self, (size_t)waitables.Count());

		if(self->shutdown)
		{
			self->shutdown = false;
			throw shutdown_t();
		}

		usys_t n_non_null = 0;
		for(auto* waitable : waitables)
			if(waitable != nullptr)
				n_non_null++;

		if(waitables.Count() == 0 || n_non_null == 0)
			return;

		self->blocked_by = waitables;
		self->state = EFiberState::BLOCKED;
		TFiber::Schedule();
		self->blocked_by = array_t<const IWaitable*>();
	}

	TFiber::TFiber() : thread(TThread::Self()), sz_stack(0), p_stack(nullptr), blocked_by(), state(EFiberState::CONSTRUCTED), stack_allocator(EStackAllocator::USER), shutdown(false)
	{
		memset(&this->eh_state, 0, sizeof(this->eh_state));
	}

	TFiber::TFiber(TThread* const thread) : thread(thread), sz_stack(0), p_stack(nullptr), blocked_by(), state(EFiberState::ACTIVE), stack_allocator(EStackAllocator::USER), shutdown(false)
	{
		memset(&this->eh_state, 0, sizeof(this->eh_state));
	}

	void TFiber::AllocateStack(void* const p_stack_input, const usys_t sz_stack_input, const EStackAllocator allocator)
	{
		EL_ERROR(allocator == EStackAllocator::USER, TInvalidArgumentException, "allocator", "allocator can not be set to USER - USER is only used internally when p_stack and sz_stack are set");

		if(p_stack_input != nullptr && sz_stack_input != 0 && !DEBUG)
		{
			this->stack_allocator = EStackAllocator::USER;
			this->sz_stack = sz_stack_input;
			this->p_stack = p_stack_input;
		}
		else
		{
			if(allocator == EStackAllocator::VIRTUAL_ALLOC || DEBUG)
			{
				const usys_t sz_redzone_min = DEBUG ? 512*1024 : 0;
				const usys_t n_pages_redzone = util::ModCeil<usys_t>(sz_redzone_min, PAGE_SIZE) / PAGE_SIZE;
				const usys_t sz_redzone = n_pages_redzone * PAGE_SIZE;
				const usys_t sz_stack = util::ModCeil<usys_t>(sz_stack_input, PAGE_SIZE);

				void* p_redzone_base = VirtualAlloc(sz_stack + 2 * sz_redzone, false, false, false, false);
				void* p_stack = (byte_t*)p_redzone_base + sz_redzone;
				this->stack_allocator = EStackAllocator::VIRTUAL_ALLOC;
				this->sz_stack = sz_stack;
				this->p_stack = p_stack;
				VirtualAllocAt(p_stack, sz_stack, true, true, false, false, true);
			}
			else if(allocator == EStackAllocator::MALLOC)
			{
				this->p_stack = aligned_alloc(16, sz_stack_input);
				EL_ERROR(this->p_stack == nullptr, TOutOfMemoryException, sz_stack_input);
				this->stack_allocator = EStackAllocator::MALLOC;
				this->sz_stack = sz_stack_input;
			}
			else
				EL_THROW(TLogicException);
		}
	}

	void TFiber::FreeStack()
	{
		switch(this->stack_allocator)
		{
			case EStackAllocator::USER:
				// nothing to do
				break;
			case EStackAllocator::MALLOC:
				free(this->p_stack);
				break;
			case EStackAllocator::VIRTUAL_ALLOC:
			{
				const usys_t sz_redzone_min = DEBUG ? 512*1024 : 0;
				const usys_t n_pages_redzone = util::ModCeil<usys_t>(sz_redzone_min, PAGE_SIZE) / PAGE_SIZE;
				const usys_t sz_redzone = n_pages_redzone * PAGE_SIZE;
				void* p_redzone_base = (byte_t*)this->p_stack - sz_redzone;
				VirtualFree(p_redzone_base, this->sz_stack + 2 * sz_redzone);
				break;
			}
		}

		this->p_stack = nullptr;
		this->sz_stack = 0;
	}

	TFiber::TFiber(TFunction<void> main_func, const bool autostart, const usys_t sz_stack, void* const p_stack) : thread(TThread::Self()), main_func(main_func), sz_stack(0), p_stack(nullptr), blocked_by(), state(EFiberState::CONSTRUCTED), shutdown(false)
	{
		IF_DEBUG_PRINTF("TFiber@%p::TFiber(main_func=?, autostart=%s, sz_stack=%zu, p_stack=%p)\n", this, autostart ? "true":"false", (size_t)sz_stack, p_stack);
		memset(&this->eh_state, 0, sizeof(this->eh_state));
		AllocateStack(p_stack, sz_stack, DEFAULT_STACK_ALLOCATOR);
		if(autostart)
			this->Start();
	}

	TFiber::~TFiber()
	{
		IF_DEBUG_PRINTF("TFiber@%p::~TFiber(): self=%p ENTER\n", this, TFiber::Self());
		if(this != &this->thread->main_fiber)
		{
			if(this->state != EFiberState::CONSTRUCTED)
			{
				// TODO: print warning
				try
				{
					this->Shutdown();
					std::unique_ptr<const IException> e = this->Join();
					if(e)
					{
						e->Print("fiber terminated with exception");
					}
				}
				catch(const IException& e)
				{
					e.Print("exception during fiber shutdown");
				}
			}

			FreeStack();
		}
		IF_DEBUG_PRINTF("TFiber@%p::~TFiber(): self=%p END\n", this, TFiber::Self());
	}

	void TFiber::Boot()
	{
		TFiber* const self = TThread::Self()->ActiveFiber();
		#ifdef __SANITIZE_ADDRESS__
			__sanitizer_finish_switch_fiber(nullptr, (const void**)&self->thread->previous_fiber->p_stack, (size_t*)&self->thread->previous_fiber->sz_stack);
		#endif

		IF_DEBUG_PRINTF("TFiber@%p::Boot(): ENTER\n", self);

		try
		{
			self->main_func();
			self->state = EFiberState::FINISHED;
			IF_DEBUG_PRINTF("TFiber@%p::Boot(): fiber main exited\n", self);
		}
		catch(shutdown_t)
		{
			IF_DEBUG_PRINTF("TFiber@%p::Boot(): caught shutdown_t\n", self);
			self->state = EFiberState::FINISHED;
		}
		catch(const IException& e)
		{
			IF_DEBUG_PRINTF("TFiber@%p::Boot(): caught IException @%p from %s:%d\n", self, &e, e.file, e.line);
			self->exception = std::unique_ptr<const IException>(e.Clone());
			self->state = EFiberState::CRASHED;
		}
		catch(const IException* e)
		{
			IF_DEBUG_PRINTF("TFiber@%p::Boot(): caught IException @%p from %s:%d\n", self, e, e->file, e->line);
			self->exception = std::unique_ptr<const IException>(e);
			self->state = EFiberState::CRASHED;
		}
		catch(...)
		{
			IF_DEBUG_PRINTF("TFiber@%p::Boot(): caught unknown exception\n", self);
			self->exception = std::unique_ptr<const IException>(new TUnknownException());
			self->state = EFiberState::CRASHED;
		}

		self->shutdown = false;
		self->thread->fibers.RemoveItem(self, NEG1);
		IF_DEBUG_PRINTF("TFiber@%p::Boot(): terminating, calling scheduler\n", self);
		TFiber::Schedule();
	}

	void TFiber::Schedule()
	{
		TThread* const thread = TThread::Self();
		EL_ERROR(thread->active_fiber == nullptr, TLogicException);
		TFiber* const self = thread->active_fiber;
		auto& fibers = thread->fibers;
		TFiber* next_fiber = nullptr;
		IF_DEBUG_PRINTF("TFiber@%p::Schedule(): ENTER\n", self);

		if(self->shutdown)
		{
			self->shutdown = false;
			throw shutdown_t();
		}

		// find another ready fiber
		if(next_fiber == nullptr)
		{
			for(TFiber* fiber : fibers)
			{
				EL_ERROR(thread != fiber->thread, TLogicException);

				if(fiber == self)
					continue;

				if(fiber->state == EFiberState::READY)
				{
					next_fiber = fiber;
					break;
				}
			}
		}

		// check-unblock blocked fibers
		// this will target all non-THandleWaitables
		if(next_fiber == nullptr)
		{
			for(TFiber* fiber : fibers)
			{
				if(fiber->state == EFiberState::BLOCKED)
				{
					for(const IWaitable* waitable : fiber->blocked_by)
						if(waitable != nullptr)
						{
							waitable->Reset();
							if(waitable->IsReady())
							{
								fiber->state = EFiberState::READY;
							}
						}

					if(fiber->state == EFiberState::READY)
					{
						next_fiber = fiber;
						break;
					}
				}
			}
		}

		// check THandleWaitables
		if(next_fiber == nullptr)
		{
			// this function only returns after some waitable became ready or a shutdown signal was received
			IF_DEBUG_PRINTF("TFiber@%p::Schedule(): calling KernelWaitForMany(fibers=%zu)\n", self, (size_t)fibers.Count());
			KernelWaitForMany(fibers);
			IF_DEBUG_PRINTF("TFiber@%p::Schedule(): returned from KernelWaitForMany(fibers=%zu)\n", self, (size_t)fibers.Count());

			for(TFiber* fiber : fibers)
			{
				if(fiber->state == EFiberState::BLOCKED)
				{
					for(const IWaitable* waitable : fiber->blocked_by)
						if(waitable != nullptr)
						{
							if(waitable->IsReady())
							{
								IF_DEBUG_PRINTF("TFiber@%p::Schedule(): fiber=%p is now READY\n", self, fiber);
								fiber->state = EFiberState::READY;
								break;
							}
						}
				}

				if(fiber->state == EFiberState::READY || fiber->state == EFiberState::ACTIVE)
				{
					next_fiber = fiber;
					// we do not break here - we want to process all waitables and update all fibers
					// to process all information gained from the expensive kernel call
				}
			}

			// this can only happen if KernelWaitForMany() has a waitable has a bug
			EL_ERROR(next_fiber == nullptr, TLogicException);
		}

		IF_DEBUG_PRINTF("TFiber@%p::Schedule(): next_fiber=%p\n", self, next_fiber);

		if(next_fiber != self)
			next_fiber->SwitchTo();

		self->state = EFiberState::ACTIVE;
		thread->active_fiber = self;

		if(self->shutdown)
		{
			self->shutdown = false;
			throw shutdown_t();
		}

		IF_DEBUG_PRINTF("TFiber@%p::Schedule(): RETURN\n", self);
		return;
	}

	void TFiber::Yield()
	{
		IF_DEBUG_PRINTF("TFiber@%p::Yield()\n", Self());
		TFiber::Schedule();
	}

	extern "C" void SwapRegisters(context_registers_t* const current, const context_registers_t* const target) noexcept asm ("__SwapRegisters__");

	void TFiber::SwitchTo()
	{
		EL_ERROR(TThread::Self() != this->thread, TLogicException);
		EL_ERROR(this->state == EFiberState::ACTIVE || this == TThread::Self()->active_fiber, TLogicException);
		EL_ERROR(!this->IsAlive(), TLogicException);	// TODO better exception

		TFiber* const self = TThread::Self()->active_fiber;
		IF_DEBUG_PRINTF("SwitchTo(): switching from=%p to=%p\n", self, this);
		EL_ERROR(self->thread != this->thread, TLogicException);

		this->thread->active_fiber = this;
		this->thread->previous_fiber = self;

		if(this->state == EFiberState::STOPPED)
			this->thread->fibers.Append(this);

		this->state = EFiberState::ACTIVE;

		if(self->state == EFiberState::ACTIVE)
			self->state = EFiberState::READY;

		#ifdef __SANITIZE_ADDRESS__
			void* fake_stack_save = nullptr;
			__sanitizer_start_switch_fiber(self->IsAlive() ? &fake_stack_save : nullptr, this->p_stack, this->sz_stack);
		#endif

		#ifdef EL1_WITH_VALGRIND
			auto stack_id = VALGRIND_STACK_REGISTER(p_stack, (byte_t*)p_stack + sz_stack);
		#endif

		// abrakadabra...
		SwapRegisters(&self->registers, &this->registers);

		// NOTE: "this" and "previous_fiber" might not exist any more (shutdown and destructed)
		// thus no access to members of this and previous_fiber must happen past SwapRegisters()

		#ifdef EL1_WITH_VALGRIND
			VALGRIND_STACK_DEREGISTER(stack_id);
		#endif

		#ifdef __SANITIZE_ADDRESS__
			__sanitizer_finish_switch_fiber(fake_stack_save, nullptr, nullptr);
		#endif

		IF_DEBUG_PRINTF("SwitchTo(): returned from=%p to=%p (my last switch was to %p)\n", self->thread->previous_fiber, self, this);

		if(self->shutdown)
		{
			self->shutdown = false;
			throw shutdown_t();
		}
	}

	void TFiber::Start()
	{
		IF_DEBUG_PRINTF("TFiber@%p::Start()\n", this);
		EL_ERROR(TThread::Self() != this->thread, TLogicException);
		EL_ERROR(this->state != EFiberState::CONSTRUCTED, TLogicException);	// TODO better exception
		this->InitRegisters();
		this->state = EFiberState::READY;
		this->thread->fibers.Append(this);
	}

	void TFiber::Start(TFunction<void> new_main_func, const usys_t sz_stack_input, void* const p_stack_input)
	{
		IF_DEBUG_PRINTF("TFiber@%p::Start(main_func=?, sz_stack_input=%zu, p_stack_input=%p)\n", this, (size_t)sz_stack_input, p_stack_input);
		EL_ERROR(TThread::Self() != this->thread, TLogicException);
		EL_ERROR(this->state != EFiberState::CONSTRUCTED, TLogicException);	// TODO better exception

		if( (sz_stack_input != this->sz_stack) || (p_stack_input != nullptr) )
		{
			FreeStack();
			AllocateStack(p_stack_input, sz_stack_input, DEFAULT_STACK_ALLOCATOR);
		}

		this->main_func = new_main_func;
		this->Start();
	}

	void TFiber::Stop()
	{
		IF_DEBUG_PRINTF("TFiber@%p::Stop(): ENTER\n", this);
		EL_ERROR(TThread::Self() != this->thread, TLogicException);
		EL_ERROR(!this->IsAlive(), TLogicException);	// TODO better exception
		TFiber* const self = TThread::Self()->active_fiber;

		if(this->state != EFiberState::STOPPED)
		{
			EL_ERROR(this->thread->fibers.RemoveItem(this, NEG1) != 1, TLogicException);
			this->state = EFiberState::STOPPED;
		}

		if(this == self)
			TFiber::Schedule();

		IF_DEBUG_PRINTF("TFiber@%p::Stop(): RETURN\n", this);
	}

	void TFiber::Resume()
	{
		IF_DEBUG_PRINTF("TFiber@%p::Resume()\n", this);
		EL_ERROR(TThread::Self() != this->thread, TLogicException);
		EL_ERROR(this->state == EFiberState::ACTIVE || this == this->thread->active_fiber, TLogicException);
		EL_ERROR(!this->IsAlive(), TLogicException);	// TODO better exception

		if(this->state == EFiberState::STOPPED)
		{
			// we put the fiber in READY state even though it might have been BLOCKED earlier when it was stopped
			// the scheduler will activate the fiber, but the WaitFor() function will put it back to BLOCKED if needed
			this->state = EFiberState::READY;
			this->thread->fibers.Append(this);
		}
	}

	// FIXME: this functiuon does not make sense this way
	bool TFiber::Kill()
	{
		IF_DEBUG_PRINTF("TFiber@%p::Kill()\n", this);
		EL_ERROR(TThread::Self() != this->thread, TLogicException);
		EL_ERROR(this->state == EFiberState::ACTIVE || this == this->thread->active_fiber, TLogicException);
		if(!this->IsAlive())
			return false;

		if(this->state == EFiberState::BLOCKED || this->state == EFiberState::READY)
			EL_ERROR(this->thread->fibers.RemoveItem(this, NEG1) != 1, TLogicException);

		this->state = EFiberState::KILLED;

		return true;
	}

	bool TFiber::Shutdown()
	{
		IF_DEBUG_PRINTF("TFiber@%p::Shutdown()\n", this);
		EL_ERROR(TThread::Self() != this->thread, TLogicException);
		if(!this->IsAlive())
			return false;

		TFiber* const self = TThread::Self()->active_fiber;

		if(this->state == EFiberState::STOPPED)
		{
			this->thread->fibers.Append(this);
		}

		// unblock / resume any fiber marked for shutdown
		if(this->state != EFiberState::ACTIVE)
			this->state = (self == this) ? EFiberState::ACTIVE : EFiberState::READY;

		this->shutdown = true;

		// do not throw shutdown_t here yet, otherwise it might throw-up in Schedule()

		return true;
	}

	TFiber* TFiber::Self()
	{
		return TThread::Self()->ActiveFiber();
	}

	std::unique_ptr<const IException> TFiber::Join()
	{
		IF_DEBUG_PRINTF("TFiber@%p::Join()\n", this);
		EL_ERROR(TThread::Self() != this->thread, TLogicException);
		EL_ERROR(this->state == EFiberState::ACTIVE || this == this->thread->active_fiber, TLogicException);
		if(this->state == EFiberState::CONSTRUCTED)
			return nullptr;

		while(this->IsAlive())
		{
			if(this->state == EFiberState::BLOCKED)
			{
				TFiberStateWaitable on_fiber_state_change(this, this->state);
				on_fiber_state_change.WaitFor();
			}
			else
				this->SwitchTo();
		}

		this->state = EFiberState::CONSTRUCTED;

		return std::move(this->exception);
	}

	EChildState TFiber::ChildState() const
	{
		switch(this->state)
		{
			case EFiberState::CONSTRUCTED: return EChildState::CONSTRUCTED;
			case EFiberState::READY:       return EChildState::ALIVE;
			case EFiberState::ACTIVE:      return EChildState::ALIVE;
			case EFiberState::BLOCKED:     return EChildState::ALIVE;
			case EFiberState::STOPPED:     return EChildState::ALIVE;
			case EFiberState::FINISHED:    return EChildState::FINISHED;
			case EFiberState::CRASHED:     return EChildState::KILLED;
			case EFiberState::KILLED:      return EChildState::KILLED;
		}

		EL_THROW(TLogicException);
	}

	ETaskState TFiber::TaskState() const
	{
		switch(this->state)
		{
			case EFiberState::CONSTRUCTED: return ETaskState::NOT_CREATED;
			case EFiberState::READY:       return ETaskState::RUNNING;
			case EFiberState::ACTIVE:      return ETaskState::RUNNING;
			case EFiberState::BLOCKED:     return ETaskState::BLOCKED;
			case EFiberState::STOPPED:     return ETaskState::STOPPED;
			case EFiberState::FINISHED:    return ETaskState::ZOMBIE;
			case EFiberState::CRASHED:     return ETaskState::ZOMBIE;
			case EFiberState::KILLED:      return ETaskState::ZOMBIE;
		}

		EL_THROW(TLogicException);
	}

	void TFiber::Sleep(const TTime time, const EClock clock)
	{
		const TTime ts_now = TTime::Now(clock);
		const TTime ts_wait_until = ts_now + time;
		TTimeWaitable waitable(clock, ts_wait_until);
		waitable.WaitFor();
	}

	/////////////////////////////////////////////////////////////

	TSortedMap<fd_t, TProcess::EFDIO> TProcess::StdioStreams()
	{
		TSortedMap<fd_t, TProcess::EFDIO> map;
		map.Add(0, TProcess::EFDIO::INHERIT);
		map.Add(1, TProcess::EFDIO::INHERIT);
		map.Add(2, TProcess::EFDIO::INHERIT);
		return map;
	}

	ISource<byte_t>* TProcess::ReceiveStream(const fd_t fd)
	{
		return streams.Get(fd);
	}

	ISink<byte_t>* TProcess::SendStream(const fd_t fd)
	{
		return streams.Get(fd);
	}

	TProcess::TProcess(
			const TPath& exe,
			const TList<TString>& args,
			const TSortedMap<fd_t, const EFDIO>& streams,
			const TSortedMap<TString, const TString>& env
		) : pid(-1), on_terminate({.read = true, .write = false, .other = false})
	{
		Start(exe, args, streams, env);
	}

	TProcess::TProcess() : pid(-1), on_terminate({.read = true, .write = false, .other = false})
	{
	}

	TProcess::~TProcess()
	{
		if(TaskState() != ETaskState::NOT_CREATED)
		{
			try
			{
				Shutdown();
				Resume();
				int status;
				EL_WARN((status = Join()) != 0 && WARN_NONZERO_EXIT_IN_DESTRUCTOR, TException, TString::Format(L"subprocess PID=%d exited with code=%d", pid, status));
			}
			catch(const IException& e)
			{
				e.Print("error during child process shuttdown");
			}
		}
	}

	static const auto STREAM_FEEDER_FUNC = [](TProcess::TSink* const dst, const TString* const src) {
		EL_ERROR(src == nullptr, TInvalidArgumentException, "src", "src must not be nullptr");
		EL_ERROR(dst == nullptr, TInvalidArgumentException, "dst", "dst must not be nullptr");
		src->chars.Pipe().Transform(io::text::encoding::TCharEncoder()).ToStream(*dst);
		if(!dst->CloseOutput())
			dst->Close();
	};

	static const auto STREAM_READER_FUNC = [](TProcess::TSource* const src, TString* const dst) {
		EL_ERROR(src == nullptr, TInvalidArgumentException, "src", "src must not be nullptr");
		EL_ERROR(dst == nullptr, TInvalidArgumentException, "dst", "dst must not be nullptr");
		src->Pipe().Transform(io::text::encoding::TCharDecoder()).AppendTo(dst->chars);
		if(!src->CloseInput())
			src->Close();
	};

	bool TProcess::IsAlive() const
	{
		switch(TaskState())
		{
			case ETaskState::NOT_CREATED:
			case ETaskState::ZOMBIE:
				return false;
			case ETaskState::RUNNING:
			case ETaskState::STOPPED:
			case ETaskState::BLOCKED:
				return true;
		}
		EL_THROW(TLogicException);
	}

	process_id_t TProcess::PID() const
	{
		return pid;
	}

	TString TProcess::Execute(const TPath& exe, const TList<TString>& args, const TString* const stdin, TString* const _stderr, const TTime timeout)
	{
		TSortedMap<fd_t, TProcess::EFDIO> fdmap;
		fdmap.Add(0, stdin == nullptr ? TProcess::EFDIO::EMPTY : TProcess::EFDIO::PIPE_PARENT_TO_CHILD);
		fdmap.Add(1, TProcess::EFDIO::PIPE_CHILD_TO_PARENT);
		fdmap.Add(2, TProcess::EFDIO::PIPE_CHILD_TO_PARENT);

		TProcess proc(exe, args, fdmap);

		TString stdout;
		TString stderr;
		TFiber stdin_feeder([&](){ STREAM_FEEDER_FUNC(proc.SendStream(0), stdin); }, stdin != nullptr);
		TFiber stdout_reader([&](){ STREAM_READER_FUNC(proc.ReceiveStream(1), &stdout); });
		TFiber stderr_reader([&](){ STREAM_READER_FUNC(proc.ReceiveStream(2), &stderr); });

		try
		{
			const TTime ts_until = timeout >= 0 ? (TTime::Now(EClock::MONOTONIC) + timeout) : -1;
			while(proc.IsAlive())
				EL_ERROR(!proc.OnTerminate().WaitFor(ts_until, true), TTimeoutException, exe, args, stderr, proc.pid, timeout);

			const process_id_t pid = proc.pid;
			const int exit_code = proc.Join();

			if(auto e = stdin_feeder.Join())  EL_FORWARD(*e, TLogicException);
			if(auto e = stdout_reader.Join()) EL_FORWARD(*e, TLogicException);
			if(auto e = stderr_reader.Join()) EL_FORWARD(*e, TLogicException);

			if(_stderr != nullptr)
				*_stderr = std::move(stderr);

			EL_ERROR(exit_code != 0, TNonZeroExitException, exe, args, stderr, pid, exit_code);
			return stdout;
		}
		catch(const IException& e)
		{
			proc.Shutdown();
			if(stdin_feeder.Join())  {}
			if(stdout_reader.Join()) {}
			if(stderr_reader.Join()) {}
			if(_stderr != nullptr)
				*_stderr = stderr;
			throw;
		}
	}

	TProcess::ISubProcessException::ISubProcessException(const io::file::TPath& exe, const TArgs& args, io::text::string::TString stderr, const process_id_t pid) : exe(exe), args(args), stderr(std::move(stderr)), pid(pid) {}

	TString TProcess::TNonZeroExitException::Message() const
	{
		TString s = TString::Format("%q", exe.ToString());
		for(auto& a : args)
			s += TString::Format(" %q", a);
		return TString::Format("subprocess %d (%s) terminated with exit-code %d; stderr output:\n%s", pid, s, exit_code, stderr);
	}

	IException* TProcess::TNonZeroExitException::Clone() const
	{
		return new TNonZeroExitException(*this);
	}

	TProcess::TNonZeroExitException::TNonZeroExitException(const io::file::TPath& exe, const TArgs& args, io::text::string::TString stderr, const process_id_t pid, const int exit_code) : ISubProcessException(exe, args, std::move(stderr), pid), exit_code(exit_code)
	{
	}

	TString TProcess::TTimeoutException::Message() const
	{
		TString s = TString::Format("%q", exe.ToString());
		for(auto& a : args)
			s += TString::Format(" %q", a);
		return TString::Format("subprocess %d (%s) exeeded timeout of %d seconds; stderr output:\n%s", pid, s, timeout.ConvertToF(EUnit::SECONDS), stderr);
	}

	IException* TProcess::TTimeoutException::Clone() const
	{
		return new TTimeoutException(*this);
	}

	TProcess::TTimeoutException::TTimeoutException(const io::file::TPath& exe, const TArgs& args, io::text::string::TString stderr, const process_id_t pid, const TTime timeout) : ISubProcessException(exe, args, std::move(stderr), pid), timeout(timeout)
	{
	}

	bool TProcess::WARN_NONZERO_EXIT_IN_DESTRUCTOR = true;
}

#undef IF_DEBUG_PRINTF
