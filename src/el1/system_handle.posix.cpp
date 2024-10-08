#include "def.hpp"
#ifdef EL_OS_CLASS_POSIX

#include "system_handle.hpp"
#include "error.hpp"
#include <unistd.h>
#include <fcntl.h>

namespace el1::system::handle
{
	void THandle::Close()
	{
		if(this->handle != -1)
		{
			const handle_t fd_swap = this->handle;
			this->handle = -1;
			EL_SYSERR(::close(fd_swap));
		}
	}

	THandle::operator handle_t() const
	{
		return this->handle;
	}

	THandle& THandle::operator=(THandle&& rhs)
	{
		this->Close();
		this->handle = rhs.handle;
		rhs.handle = -1;
		return *this;
	}

	THandle& THandle::operator=(const THandle& rhs)
	{
		this->Close();
		this->handle = EL_SYSERR(::fcntl(rhs.handle, F_DUPFD_CLOEXEC, 3));
		return *this;
	}

	THandle& THandle::operator=(const handle_t rhs)
	{
		this->Close();
		this->handle = rhs;
		return *this;
	}

	handle_t THandle::Claim()
	{
		const handle_t handle_tmp = this->handle;
		this->handle = -1;
		return handle_tmp;
	}

	void THandle::BlockingIO(bool block)
	{
		const int old_flags = EL_SYSERR(fcntl(handle, F_GETFL));
		int new_flags = old_flags;

		if(block)
			new_flags &= (~O_NONBLOCK);
		else
			new_flags |= O_NONBLOCK;

		if(new_flags != old_flags)
			EL_SYSERR(fcntl(handle, F_SETFL, new_flags));
	}

	bool THandle::BlockingIO() const
	{
		return (EL_SYSERR(fcntl(handle, F_GETFL)) & O_NONBLOCK) == 0;
	}

	THandle::~THandle()
	{
		if(this->handle != -1)
			::close(this->handle);
	}

	THandle::THandle() : handle(-1)
	{
	}

	THandle::THandle(THandle&& other) : handle(other.handle)
	{
		other.handle = -1;
	}

	THandle::THandle(const THandle& other)
	{
		this->handle = EL_SYSERR(::fcntl(other.handle, F_DUPFD_CLOEXEC, 0));
	}

	THandle::THandle(const handle_t handle, const bool claim) : handle(claim ? handle : EL_SYSERR(::fcntl(handle, F_DUPFD_CLOEXEC, 0)))
	{
	}
}

#endif
