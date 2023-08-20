#pragma once

#include "def.hpp"
#include "system_handle.hpp"
#include "system_time.hpp"
#include "system_waitable.hpp"
#include "io_types.hpp"
#include "io_collection_list.hpp"
#include "io_collection_map.hpp"
#include "io_file.hpp"
#include "io_text_string.hpp"
#include "util_function.hpp"
#include "error.hpp"
#include <optional>

namespace __cxxabiv1
{
	struct __cxa_exception;

	struct __cxa_eh_globals
	{
		__cxa_exception* caughtExceptions;
		unsigned int uncaughtExceptions;

		#if defined(__arm__) || defined(__aarch64__)
			__cxa_exception* propagatingExceptions;
		#endif
	};

	extern "C" __cxa_eh_globals* __cxa_get_globals();
}


namespace el1::system::task
{
	using namespace io::text::string;
	using namespace io::collection::list;
	using namespace io::types;
	using namespace util::function;
	using namespace error;
	using namespace system::time;
	using namespace system::handle;
	using namespace system::waitable;

	class TThread;
	class TFiber;

	using process_id_t = s32_t;

	enum class EChildState : u8_t
	{
		CONSTRUCTED,	// the constructor was called and all data structures were initialized
		ALIVE,			// the child is alive (check task state to find more details)
		FINISHED,		// the child successfully completed execution and shut itself down
		KILLED,			// the child was killed (possibly due to an exception or another error)
		JOINING			// a thread is currently waiting to join with the child
	};

	enum class ETaskState : u8_t
	{
		NOT_CREATED,	// the process was not created yet or it terminated and the parent joined with it - in any case it is gone or never existed in the first place
		RUNNING,		// the process is scheduled for execution
		STOPPED,		// the process was stopped and will not be scheduled for execution any more until it is resumed by an external event
		BLOCKED,		// the process is waiting for a resource to become available
		ZOMBIE			// the process terminated and the parent now needs to join with it
	};

	enum class EFiberState : u8_t
	{
		CONSTRUCTED,	// the constructor was called and all data structures were initialized, but the fiber was not started yet
		READY,			// the fiber is alive and ready to run, but currently another fiber is active
		ACTIVE,			// the fiber is alive and currently the active fiber on the thread
		BLOCKED,		// the fiber is alive, but waiting for a resource to become available (SwitchTo() will throw)
		STOPPED,		// the fiber is alive, but stopped and will not be scheduled for execution any more (but SwitchTo() still works)
		FINISHED,		// the fiber successfully completed execution and shut itself down
		CRASHED,		// the fiber died with an uncaught exception
		KILLED,
	};

	struct TInvalidChildStateException : IException
	{
		const EChildState expected_state;
		const EChildState actual_state;
		const bool inverted;

		TString Message() const final override;
		IException* Clone() const override;

		static const char* StateMnemonic(const EChildState state);

		TInvalidChildStateException(const bool inverted, const EChildState expected_state, const EChildState actual_state) : expected_state(expected_state), actual_state(actual_state), inverted(inverted) {}
	};

	struct ITask
	{
		virtual void Stop() = 0;
		virtual void Resume() = 0;
		virtual bool Kill() = 0;
		virtual bool Shutdown() = 0;
		virtual ETaskState TaskState() const EL_WARN_UNUSED_RESULT = 0;
	};

	struct IChildTask : ITask
	{
		virtual std::unique_ptr<const IException> Join() EL_WARN_UNUSED_RESULT = 0;
		virtual EChildState ChildState() const EL_WARN_UNUSED_RESULT = 0;
	};

	struct IMutex
	{
		void Accquire() { EL_ERROR(!Accquire(-1), TLogicException); }
		virtual bool Accquire(const TTime timeout) = 0;
		virtual void Release() = 0;
		virtual bool IsAcquired() const EL_GETTER = 0;
	};

	struct IInterlocked
	{
		virtual IMutex& Mutex() const = 0;
	};

	struct ISignal : IInterlocked
	{
		virtual void Raise() = 0;
	};

	class TIpcMutex : public IMutex
	{
		protected:
			THandle handle;

		public:
			bool Accquire(const TTime timeout) final override;
			void Release() final override;
			bool IsAcquired() const final override EL_GETTER;

	};

	class TIpcSignal : public ISignal, public THandleWaitable
	{
		protected:
			mutable TIpcMutex mutex;

		public:
			TIpcMutex& Mutex() const final override { return mutex; }

			void Raise() final override;
	};

	class TSimpleMutex : public IMutex
	{
		protected:
			friend class TSimpleSignal;
			void* const data;
			const TThread* owner;
			unsigned n_accquire;

		public:
			bool Accquire(const TTime timeout) final override;
			void Release() final override;
			bool IsAcquired() const final override EL_GETTER;

			TSimpleMutex(TSimpleMutex&&) = delete;
			TSimpleMutex(const TSimpleMutex&) = delete;
			TSimpleMutex();
			~TSimpleMutex();
	};


	class TFiberMutex : public IMutex
	{
		protected:
			TFiber* owner;
			unsigned n_accquire;

		public:
			bool Accquire(const TTime timeout) final override;
			void Release() final override;
			bool IsAcquired() const final override;

			TFiberMutex();
			~TFiberMutex();
	};

	class TSimpleSignal : public ISignal
	{
		protected:
			TSimpleMutex* const mutex;
			void* const data;

		public:
			TSimpleMutex& Mutex() const final override { return *mutex; }

			bool WaitFor(const TTime timeout) const;
			void Raise() final override;

			TSimpleSignal(TSimpleSignal&&) = delete;
			TSimpleSignal(const TSimpleSignal&) = delete;
			TSimpleSignal(TSimpleMutex* const mutex);
			~TSimpleSignal();
	};

	struct TMutexAutoLock
	{
		IMutex* const mutex;

		TMutexAutoLock(TMutexAutoLock&&) = default;
		TMutexAutoLock(const TMutexAutoLock&) = delete;

		TMutexAutoLock(IMutex* const mutex) : mutex(mutex)
		{
			mutex->Accquire();
		}

		~TMutexAutoLock()
		{
			if(mutex->IsAcquired())
				mutex->Release();
		}
	};

	template<typename T>
	class TAcquiredMutexPtr
	{
		protected:
			TMutexAutoLock al;
			T* object;

		public:
			TAcquiredMutexPtr(TAcquiredMutexPtr&&) = default;
			TAcquiredMutexPtr(const TAcquiredMutexPtr&) = delete;

			T* operator->() EL_GETTER { return object; }
			T& operator*() EL_GETTER { return *object; }

			const T* operator->() const EL_GETTER { return object; }
			const T& operator*() const EL_GETTER { return *object; }

			TAcquiredMutexPtr(IMutex* const mutex, T* const object) : al(mutex), object(object) {}
	};

	template<typename T>
	class TMutexPtr
	{
		protected:
			IMutex* const mutex;
			T* const object;

		public:
			TAcquiredMutexPtr<T> Acquire()
			{
				return TAcquiredMutexPtr<T>(mutex, object);
			}

			TMutexPtr(IMutex* const mutex, T* const object);
	};

	#ifdef EL_OS_LINUX
		#if defined(__x86_64__)
			struct context_registers_t
			{
				union
				{
					u64_t arr[8];
					struct
					{
						u64_t rbx;
						u64_t rbp;
						u64_t r12;
						u64_t r13;
						u64_t r14;
						u64_t r15;
						u64_t rsp;
						u64_t rip;
					};
				};
			};
			#define EL_ARCH_STACKPOINTER_REGISTER_NAME rsp
		#elif defined(__arm__) && !defined(__aarch64__)
			struct context_registers_t
			{
				union
				{
					u32_t arr[10];
					struct
					{
						u32_t r4;
						u32_t r5;
						u32_t r6;
						u32_t r7;
						u32_t r8;
						u32_t r9;
						u32_t r10;
						u32_t r11;
						u32_t r13;
						u32_t r15;
					};
				};
			};
			#define EL_ARCH_STACKPOINTER_REGISTER_NAME r13
		#elif defined(__aarch64__)
			struct context_registers_t
			{
				union
				{
					u32_t arr[13];
					struct
					{
						u64_t x19;
						u64_t x20;
						u64_t x21;
						u64_t x22;
						u64_t x23;
						u64_t x24;
						u64_t x25;
						u64_t x26;
						u64_t x27;
						u64_t x28;
						u64_t x29;
						u64_t lr;
						u64_t sp;
					};
				};
			};
			#define EL_ARCH_STACKPOINTER_REGISTER_NAME sp
		#else
			#error "unsupported CPU architecture"
		#endif
	#else
		#error "unsupported platform"
	#endif


	enum class EStackAllocator : u8_t
	{
		USER, // DO NOT USE - this is only used internally when p_stack and sz_stack were specified
		MALLOC, // DEFAULT
		VIRTUAL_ALLOC // SLOWER, but more memory effiecient for large sparsely used stacks
	};

	// fibers are "lightweight threads" that are based on cooperative multitasking
	// they strictly belong to the thread that constructed them and must not be manipulated by other threads
	// any TThread always runs exactly one fiber at a time, the active fiber decides by itself when to switch to a different fiber
	// the scheduler is only used when the active fiber blocks or diliberately calls the scheduler, there is no time-slicing or other forced switching
	// switching fibers is a very lightweight process and does not require any system calls
	class TFiber : public IChildTask
	{
		friend class TThread;
		protected:
			TThread* const thread;
			TFunction<void> main_func;
			usys_t sz_stack;
			void* p_stack;
			std::unique_ptr<const IException> exception;
			array_t<const IWaitable*> blocked_by;
			EFiberState state;
			EStackAllocator stack_allocator;
			bool shutdown;
			context_registers_t registers;
			__cxxabiv1::__cxa_eh_globals eh_state;

			static void Boot();
			static void Schedule();
			void InitRegisters();
			void AllocateStack(void* const p_stack_input, const usys_t sz_stack_input, const EStackAllocator allocator);
			void FreeStack();

			TFiber(TThread* const thread);

		private:
			static void KernelWaitForMany(const TList<TFiber*>& fibers);

		public:
			static bool DEBUG; // do NOT change after fibers were created
			static EStackAllocator DEFAULT_STACK_ALLOCATOR;
			static usys_t FIBER_DEFAULT_STACK_SIZE_BYTES;

			TFiber(const TFiber&) = delete;
			TFiber(TFiber&&) = delete;
			TFiber& operator=(const TFiber&) = delete;
			TFiber& operator=(TFiber&&) = delete;
			virtual ~TFiber();

			TFiber(); // won't start fiber
			TFiber(TFunction<void> main_func, const bool autostart = true, const usys_t sz_stack = FIBER_DEFAULT_STACK_SIZE_BYTES, void* const p_stack = nullptr);

			// returns the amount of free stack space in bytes for this fiber
			// naturally this might be off by a few-bytes for the function call itself if the calling fiber is inquiring on its own stack
			usys_t StackFree() const;

			// returns the amount of bytes that were allocated for the stack of this fiber
			usys_t StackTotal() const EL_GETTER { return sz_stack; }

			// returns the amount of bytes that were allocated for the stack of this fiber
			usys_t StackUsed() const { return StackTotal() - StackFree(); }

			// waits for multiple waitables at the same time
			// this will block the active fiber and call the scheduler
			// returns once at least one waitable becomes ready
			static void WaitForMany(const array_t<const IWaitable*>);
			static void WaitForMany(const std::initializer_list<const IWaitable*>);

			// calls the scheduler to find a new active fiber
			// the current fiber will be put in READY state and moved to the end of the queue
			// you should only use this function if you know for certain that there is another READY fiber
			static void Yield();

			EFiberState State() const { return this->state; }

			bool IsAlive() const { return this->state == EFiberState::ACTIVE || this->state == EFiberState::READY || this->state == EFiberState::BLOCKED || this->state == EFiberState::STOPPED; }

			bool IsJoinable() const { return this->state == EFiberState::FINISHED || this->state == EFiberState::CRASHED || this->state == EFiberState::KILLED; }

			// switches to this fiber, fails if the fiber is not alive
			void SwitchTo();

			// starts a fiber - fails if the fiber is not in CONSTRUCTED state
			void Start();
			void Start(TFunction<void> main_func, const usys_t sz_stack = FIBER_DEFAULT_STACK_SIZE_BYTES, void* const p_stack = nullptr);

			// stops the selected fiber - it will no longer be picked up by the scheduler
			// the fiber remains alive and can be the target of SwitchTo(), at which point
			// it enters ACTIVE state and is no longer considered STOPPED
			void Stop() final override;

			// if the selected fiber is in STOPPED state it will be moved into READY state, has no effect otherwise
			void Resume() final override;

			// immediately moves a fiber into KILLED state
			// this operation can only be performed by the ACTIVE fiber on itself, or onto a fiber
			// in READY, BLOCKED or STOPPED state
			// no cleanup, destructors or stack unwinding is performed in the killed fiber and will
			// thus likely put the entire program in undefined state and should therfor be avoided
			bool Kill() final override;

			// requests the fiber to gracefully shutdown
			bool Shutdown() final override;

			static void Sleep(const TTime time, const EClock clock = EClock::MONOTONIC);

			// merges with a fiber in FINISHED, CRASHED or KILLED state and returns it into CONSTRUCTED state
			// this will repeatetly call SwitchTo() while target fiber is not joinable
			std::unique_ptr<const IException> Join() final override EL_WARN_UNUSED_RESULT;

			EChildState ChildState() const final override EL_WARN_UNUSED_RESULT;
			ETaskState TaskState() const final override EL_WARN_UNUSED_RESULT;

			static TFiber* Self();
	};

	class TFiberStateWaitable : public IWaitable
	{
		public:
			const TFiber* const fiber;
			const EFiberState current_state;
			TFiberStateWaitable(const TFiber* const fiber, const EFiberState current_state) : fiber(fiber), current_state(current_state) {}

			bool IsReady() const final override { return fiber->State() != current_state; }

	};

	class TThread : public IChildTask, public IInterlocked
	{
		friend struct TMainThread;
		friend class TFiber;
		private:
			const TString name;
			mutable TSimpleMutex mutex;
			mutable TSimpleSignal on_state_change;
			void* thread_handle;
			const process_id_t constructor_pid;
			volatile process_id_t thread_pid;
			volatile process_id_t starter_pid;
			volatile process_id_t terminator_pid;
			volatile process_id_t joiner_pid;
			volatile EChildState state;
			TList<TFiber*> fibers;	// list of fibers in READY, BLOCKED or SHUTDOWN states
			TFiber main_fiber;
			TFiber* volatile active_fiber;
			TFiber* volatile previous_fiber;

			TThread();

			#ifdef EL_OS_CLASS_POSIX
				static void* PthreadMain(void*);
			#endif

		public:
			TThread(const TString name, TFunction<void> main_func, const bool autostart = true);

			process_id_t ThreadPID() const EL_GETTER { return this->thread_pid; }
			const TString& Name() const { return name; }

			void Start();
			void Stop() final override;
			void Resume() final override;
			bool Kill() final override;
			bool Shutdown() final override;
			std::unique_ptr<const IException> Join() final override EL_WARN_UNUSED_RESULT;
			EChildState ChildState() const final override EL_WARN_UNUSED_RESULT { return state; }
			ETaskState TaskState() const final override EL_WARN_UNUSED_RESULT;

			const TFiber* ActiveFiber() const { return this->active_fiber; }
			TFiber*       ActiveFiber()       { return this->active_fiber; }

			TFiber& MainFiber() { return main_fiber; }

			const TList<TFiber*>& Fibers() { return fibers; }

			TSimpleMutex& Mutex() const final override { return mutex; }
			const TSimpleSignal& OnStateChange() const { return on_state_change; }

			static TThread* Self();

			TThread(TThread&&) = delete;
			TThread(const TThread&) = delete;
			~TThread();

			static TThread& MainThread();
	};

	const io::collection::map::TSortedMap<io::text::string::TString, const io::text::string::TString>& EnvironmentVariables();

	io::collection::list::TList<fd_t> EnumOpenFileDescriptors();

	class TPipe : public io::stream::ISink<byte_t>, public io::stream::ISource<byte_t>
	{
		protected:
			THandle tx, rx;
			mutable THandleWaitable on_input_ready;
			mutable THandleWaitable on_output_ready;

		public:
			THandle& SendSide() EL_GETTER { return tx; }
			THandle& ReceiveSide() EL_GETTER { return rx; }

			usys_t Read(byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			usys_t Write(const byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;

			THandleWaitable* OnInputReady() const final override;
			THandleWaitable* OnOutputReady() const final override;

			void Close() final override;

			TPipe();
			TPipe(TPipe&&) = default;
			TPipe(const TPipe&) = delete;
	};

	class TProcess : public ITask
	{
		protected:
			process_id_t pid;
			THandle h_proc;
			mutable THandleWaitable on_terminate;
			io::collection::map::TSortedMap<fd_t, TPipe> streams;

		public:
			static bool WARN_NONZERO_EXIT_IN_DESTRUCTOR;

			enum class EFDIO : u8_t
			{
				INHERIT,	// inherit FD from parent
				EMPTY,		// read from /dev/null
				DISCARD,	// output to /dev/null
				ZERO,		// read from /dev/zero
				RANDOM,		// read from /dev/urandom
				PIPE_PARENT_TO_CHILD,	// pipe parent ISink<byte_t> to child input
				PIPE_CHILD_TO_PARENT	// pipe output from child to parent ISource<byte_t>
			};

			THandleWaitable& OnTerminate() const { return on_terminate; }
			void Start(const io::file::TPath& exe, const io::collection::list::TList<TString>& args = io::collection::list::TList<TString>(), const io::collection::map::TSortedMap<fd_t, const EFDIO>& streams = StdioStreams(), const io::collection::map::TSortedMap<TString, const TString>& env = EnvironmentVariables());
			void Stop() final override;
			void Resume() final override;
			bool Kill() final override;
			bool Shutdown() final override;
			int Join() EL_WARN_UNUSED_RESULT;


// 			const THandleWaitable& OnStateChange() const;
			ETaskState TaskState() const final override EL_WARN_UNUSED_RESULT;

			// returns nullptr if the specified fd is not mapped (or going in the other direction)
			io::stream::ISource<byte_t>* ReceiveStream(const fd_t fd) EL_GETTER;
			io::stream::ISink<byte_t>* SendStream(const fd_t fd) EL_GETTER;

			static io::collection::map::TSortedMap<fd_t, EFDIO> StdioStreams();

			TProcess(
				const io::file::TPath& exe,
				const io::collection::list::TList<TString>& args = io::collection::list::TList<TString>(),
				const io::collection::map::TSortedMap<fd_t, const EFDIO>& streams = StdioStreams(),
				const io::collection::map::TSortedMap<TString, const TString>& env = EnvironmentVariables()
			);

			TProcess();
			~TProcess();

			static io::text::string::TString Execute(const io::file::TPath& exe, const io::collection::list::TList<TString>& args = io::collection::list::TList<TString>());

			// io::collection::list::TList<memory::mapping_info_t> MemoryMappings() const;
	};

}
