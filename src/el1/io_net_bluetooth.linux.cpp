#include "io_net_bluetooth.hpp"
#ifdef EL_OS_LINUX

#include "error.hpp"
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>

namespace el1::io::net::bluetooth
{
	using namespace error;
	using namespace system::handle;
	using namespace system::time;
	using namespace system::waitable;
	using namespace text::string;


	TRfcommClient::TRfcommClient(const TString& remote_address, const u8_t channel, const TTime connect_timeout) :
		TRfcommClient(address_t(remote_address), channel, connect_timeout)
	{
	}

	TRfcommClient::TRfcommClient(const address_t remote_address, const u8_t channel, const TTime connect_timeout) :
		handle(::socket(AF_BLUETOOTH, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, BTPROTO_RFCOMM), true),
		on_rx_ready({ .read = true, .write = false, .other = false }, handle),
		on_tx_ready({ .read = false, .write = true, .other = false }, handle),
		remote_address(remote_address),
		channel(channel)
	{
		EL_ERROR(handle == INVALID_HANDLE, TSyscallException, errno);
		EL_ERROR(channel == 0 || channel > 30, TInvalidArgumentException, "channel", "must be between 1 and 30");

		struct sockaddr_rc remote = {};
		remote.rc_family = AF_BLUETOOTH;
		remote.rc_channel = channel;
		for(usys_t i = 0; i < 6; i++)
			remote.rc_bdaddr.b[i] = remote_address.octet[5 - i];

		const int result = ::connect(handle, reinterpret_cast<const sockaddr*>(&remote), sizeof(remote));
		if(result < 0)
		{
			EL_ERROR(errno != EINPROGRESS, TSyscallException, errno);
			EL_ERROR(!on_tx_ready.WaitFor(connect_timeout), TException, U"RFCOMM connection attempt timed out");
			on_tx_ready.Reset();

			int socket_error = 0;
			socklen_t error_size = sizeof(socket_error);
			EL_SYSERR(::getsockopt(handle, SOL_SOCKET, SO_ERROR, &socket_error, &error_size));
			EL_ERROR(socket_error != 0, TSyscallException, socket_error);
		}
	}

	handle_t TRfcommClient::Handle()
	{
		return handle;
	}

	usys_t TRfcommClient::Read(byte_t* const arr_items, const usys_t n_items_max)
	{
		if(on_rx_ready.Handle() == INVALID_HANDLE)
			return 0;

		const ssize_t result = ::recv(handle, arr_items, n_items_max, 0);
		if(result > 0)
			return static_cast<usys_t>(result);
		if(result == 0)
		{
			CloseInput();
			return 0;
		}
		EL_ERROR(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR, TSyscallException, errno);
		return 0;
	}

	usys_t TRfcommClient::Write(const byte_t* const arr_items, const usys_t n_items_max)
	{
		if(on_tx_ready.Handle() == INVALID_HANDLE)
			return 0;

		const ssize_t result = ::send(handle, arr_items, n_items_max, MSG_NOSIGNAL);
		if(result > 0)
			return static_cast<usys_t>(result);
		if(result == 0)
		{
			CloseOutput();
			return 0;
		}
		EL_ERROR(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR, TSyscallException, errno);
		return 0;
	}

	const THandleWaitable* TRfcommClient::OnInputReady() const
	{
		return on_rx_ready.Handle() == INVALID_HANDLE ? nullptr : &on_rx_ready;
	}

	const THandleWaitable* TRfcommClient::OnOutputReady() const
	{
		return on_tx_ready.Handle() == INVALID_HANDLE ? nullptr : &on_tx_ready;
	}

	bool TRfcommClient::CloseOutput()
	{
		if(on_tx_ready.Handle() != INVALID_HANDLE)
		{
			EL_SYSERR(::shutdown(handle, SHUT_WR));
			on_tx_ready.Handle(INVALID_HANDLE);
		}
		return true;
	}

	bool TRfcommClient::CloseInput()
	{
		if(on_rx_ready.Handle() != INVALID_HANDLE)
		{
			EL_SYSERR(::shutdown(handle, SHUT_RD));
			on_rx_ready.Handle(INVALID_HANDLE);
		}
		return true;
	}

	void TRfcommClient::Close()
	{
		handle.Close();
		on_rx_ready.Handle(INVALID_HANDLE);
		on_tx_ready.Handle(INVALID_HANDLE);
	}

	void TRfcommClient::Flush()
	{
	}
}

#endif
