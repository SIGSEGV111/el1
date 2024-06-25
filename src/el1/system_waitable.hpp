#pragma once

#include "def.hpp"
#include "system_handle.hpp"
#include "system_time.hpp"
#include "io_types.hpp"

namespace el1::system::task
{
	class TFiber;
}

namespace el1::system::waitable
{
	// a waitable monitors some asynchronous resource or process
	// once the resource enters a specific state or the process
	// completes the waitable will be marked as "ready"
	// there are different types of waitables
	// however the details are mostly irrelevant for user code
	// the only defining property is that you can "wait for something to happen"
	// waitable are offered by classes that deal with such asynchronous events
	// use them in conjunction with TFiber::WaitForMany()

	class THandleWaitable;

	struct IWaitable
	{
		void WaitFor() const;
		bool WaitFor(const time::TTime timeout, const bool absolute_time = false) const EL_WARN_UNUSED_RESULT;
		virtual bool IsReady() const = 0;
		virtual void Reset() const {}
		virtual const THandleWaitable* HandleWaitable() const { return nullptr; }
	};

	class THandleWaitable : public IWaitable
	{
		friend class task::TFiber;

		public:
			struct wait_t
			{
				bool read;
				bool write;
				bool other;
			};

		protected:
			mutable bool is_ready;
			wait_t wait;
			handle::handle_t handle;

		public:
			THandleWaitable(const wait_t wait) : is_ready(false), wait(wait) {}
			THandleWaitable(const wait_t wait, const handle::handle_t handle) : is_ready(false), wait(wait), handle(handle) {}

			wait_t Wait() const EL_GETTER { return wait; }
			handle::handle_t Handle() const EL_GETTER { return handle; }
			void Handle(const handle::handle_t new_handle) EL_SETTER { handle = new_handle; }

			bool IsReady() const final override { return is_ready; }
			void Reset() const final override { is_ready = false; }

			const THandleWaitable* HandleWaitable() const final override { return this; }
	};

	template<typename T>
	class TMemoryWaitable : public IWaitable
	{
		public:
			const T* watch;
			const T* ref;
			T mask;

			TMemoryWaitable() : watch(nullptr), ref(nullptr), mask(0) {}
			TMemoryWaitable(const T* const watch, const T* const ref, const T mask) : watch(watch), ref(ref), mask(mask) {}

			bool IsReady() const final override EL_GETTER
			{
				if(watch == nullptr)
					return false;

				if(ref == nullptr)
					return (*watch & mask) != 0;

				if(ref == (T*)io::types::NEG1)
					return (*watch & mask) == 0;

				return (*watch & mask) != (*ref & mask);
			}
	};

	enum class EWaitUntil
	{
		EQUAL,
		NOT_EQUAL
	};

	template<typename T>
	struct TStateWaitable : public IWaitable
	{
		T reference_state;
		const T* monitored_state;
		EWaitUntil wait_until;

		bool IsReady() const final override
		{
			switch(wait_until)
			{
				case EWaitUntil::EQUAL:
					return reference_state == *monitored_state;

				case EWaitUntil::NOT_EQUAL:
					return reference_state != *monitored_state;
			}

			return true;
		}

		TStateWaitable& operator=(TStateWaitable&&) = default;
		TStateWaitable& operator=(const TStateWaitable&) = default;
		TStateWaitable(TStateWaitable&&) = default;
		TStateWaitable(const TStateWaitable&) = default;

		TStateWaitable(T reference_state, const T* const monitored_state, const EWaitUntil wait_until) : reference_state(std::move(reference_state)), monitored_state(monitored_state), wait_until(wait_until) {}
	};
}
