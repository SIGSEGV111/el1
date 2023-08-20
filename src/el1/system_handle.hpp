#pragma once

#include "def.hpp"

#ifdef EL_OS_CLASS_WINDOWS
	#include <winbase.h>
#endif

namespace el1::system::handle
{
	using fd_t = int;

	#ifdef EL_OS_CLASS_POSIX
		using handle_t = fd_t;
		static const handle_t INVALID_HANDLE = -1;
	#endif

	#ifdef EL_OS_CLASS_WINDOWS
		using handle_t = HANDLE;
		static const handle_t INVALID_HANDLE = NULL;
	#endif


	class THandle
	{
		private:
			handle_t handle;

		public:
			THandle();
			THandle(THandle&& other);
			THandle(const THandle& other);
			~THandle();

			THandle& operator=(THandle&& rhs);
			THandle& operator=(const THandle& rhs);

			void BlockingIO(bool);
			bool BlockingIO() const;

			void Close();
			handle_t Claim();
			operator handle_t() const;
			THandle& operator=(const handle_t);
			THandle(const handle_t handle, const bool claim);
	};
}
