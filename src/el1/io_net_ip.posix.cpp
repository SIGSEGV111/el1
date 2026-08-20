#include "io_net_ip.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <ifaddrs.h>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

namespace el1::io::net::ip
{
	using namespace system::handle;
	using namespace system::waitable;
	using namespace system::time;
	using namespace collection::list;
	using namespace text::string;

	static sockaddr_in ConvertToPosixV4(const ipaddr_t ip, const port_t port)
	{
		EL_ERROR(!ip.IsV4(), TInvalidArgumentException, "ip", "ip must be a IPv4 address");
		struct sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = ip.IPv4();
		addr.sin_port = htons(port);
		return addr;
	}

	static sockaddr_in6 ConvertToPosixV6(const ipaddr_t ip, const port_t port)
	{
		struct sockaddr_in6 addr = {};
		addr.sin6_family = AF_INET6;
		memcpy(addr.sin6_addr.s6_addr, ip.octet, 16);
		addr.sin6_port = htons(port);
		return addr;
	}

	static sockaddr_in ConvertToPosixV4(const port_t port)
	{
		struct sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(port);
		return addr;
	}

	static sockaddr_in6 ConvertToPosixV6(const port_t port)
	{
		struct sockaddr_in6 addr = {};
		addr.sin6_family = AF_INET6;
		addr.sin6_addr = IN6ADDR_ANY_INIT;
		addr.sin6_port = htons(port);
		return addr;
	}

	static ipport_t ConvertFromPosix(const sockaddr& addr)
	{
		switch(addr.sa_family)
		{
			case AF_INET:
			{
				return {
					ipaddr_t((u32_t)reinterpret_cast<const sockaddr_in&>(addr).sin_addr.s_addr),
					ntohs(reinterpret_cast<const sockaddr_in&>(addr).sin_port)
				};
			}

			case AF_INET6:
			{
				ipaddr_t ip;
				memcpy(ip.octet, reinterpret_cast<const sockaddr_in6&>(addr).sin6_addr.s6_addr, 16);
				return { ip, ntohs(reinterpret_cast<const sockaddr_in6&>(addr).sin6_port) };
			}

			// LCOV_EXCL_START
			default:
				EL_THROW(TInvalidArgumentException, "addr", "addr must be either AF_INET or AF_INET6");
			// LCOV_EXCL_STOP
		}
	}

	static ipport_t ConvertFromPosix(const sockaddr_in6& addr)
	{
		return ConvertFromPosix((const sockaddr&)addr);
	}

	static int SocketDomain(const handle_t handle)
	{
		int domain = 0;
		socklen_t len = sizeof(domain);
		EL_SYSERR(getsockopt(handle, SOL_SOCKET, SO_DOMAIN, &domain, &len));
		return domain;
	}

	static ipport_t AddressFromSocket(handle_t handle)
	{
		socklen_t len = 0;
		switch(SocketDomain(handle))
		{
			case AF_INET:
			{
				struct sockaddr_in addr = {};
				len = sizeof(addr);
				EL_SYSERR(getsockname(handle, (sockaddr*)&addr, &len));
				return ConvertFromPosix((const sockaddr&)addr);
			}

			case AF_INET6:
			{
				struct sockaddr_in6 addr = {};
				len = sizeof(addr);
				EL_SYSERR(getsockname(handle, (sockaddr*)&addr, &len));
				return ConvertFromPosix((const sockaddr&)addr);
			}

			// LCOV_EXCL_START
			default:
				EL_THROW(TException, U"unknown address family");
			// LCOV_EXCL_STOP
		}
	}

	static THandle CreateSocket(const int type, port_t local_port, EIP version)
	{
		const int v0 = 0;
		const int v1 = 1;
		THandle handle;

		if(version == EIP::ANY)
		{
			// try IPv6 first
			if( (handle = socket(AF_INET6, type | SOCK_CLOEXEC, 0)) >= 0 )
			{
				EL_SYSERR(setsockopt(handle, IPPROTO_IPV6, IPV6_V6ONLY, &v0, sizeof(v0)));
				EL_SYSERR(setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, &v1, sizeof(v1)));

				const auto addr = ConvertToPosixV6(local_port);
				if(bind(handle, (const sockaddr*)&addr, sizeof(addr)) == 0)
				{
					return handle;
				}
			}

			// now try IPv4
			handle = EL_SYSERR(socket(AF_INET, type | SOCK_CLOEXEC, 0));
			EL_SYSERR(setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, &v1, sizeof(v1)));
			const auto addr = ConvertToPosixV4(local_port);
			EL_SYSERR(bind(handle, (const sockaddr*)&addr, sizeof(addr)));
			return handle;
		}
		else if(version == EIP::V4)
		{
			handle = EL_SYSERR(socket(AF_INET, type | SOCK_CLOEXEC, 0));
			EL_SYSERR(setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, &v1, sizeof(v1)));
			const auto addr = ConvertToPosixV4(local_port);
			EL_SYSERR(bind(handle, (const sockaddr*)&addr, sizeof(addr)));
			return handle;
		}
		else if(version == EIP::V6)
		{
			handle = EL_SYSERR(socket(AF_INET6, type | SOCK_CLOEXEC, 0));
			EL_SYSERR(setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, &v1, sizeof(v1)));
			const auto addr = ConvertToPosixV6(local_port);
			EL_SYSERR(bind(handle, (const sockaddr*)&addr, sizeof(addr)));
			return handle;
		}
		else
			EL_THROW(TLogicException); // LCOV_EXCL_LINE
	}

	static THandle CreateSocket(const int type, const ipaddr_t bind_ip, const port_t local_port)
	{
		const int v1 = 1;
		THandle handle;

		if(bind_ip.IsV4())
		{
			handle = EL_SYSERR(socket(AF_INET, type | SOCK_CLOEXEC, 0));
			EL_SYSERR(setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, &v1, sizeof(v1)));
			const auto addr = ConvertToPosixV4(bind_ip, local_port);
			EL_SYSERR(bind(handle, (const sockaddr*)&addr, sizeof(addr)));
		}
		else
		{
			handle = EL_SYSERR(socket(AF_INET6, type | SOCK_CLOEXEC, 0));
			EL_SYSERR(setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, &v1, sizeof(v1)));
			const auto addr = ConvertToPosixV6(bind_ip, local_port);
			EL_SYSERR(bind(handle, (const sockaddr*)&addr, sizeof(addr)));
		}

		return handle;
	}

	/*********************************************************************************/

	ipaddr_t::ipaddr_t(const TStringView str, const EIP version)
	{
		const auto cstr = str.MakeCStr();

		if((version == EIP::ANY && str.Contains('.')) || version == EIP::V4)
		{
			ipaddr_t ip(EIP::V4);
			EL_ERROR(inet_pton(AF_INET, cstr.get(), (char*)&ip.IPv4()) <= 0, TException, TString::Format(U"%q cannot be parsed as IPv4 address", str));
			*this = ipaddr_t(ip);
		}
		else if((version == EIP::ANY && str.Contains(':')) || version == EIP::V6)
		{
			ipaddr_t ip(EIP::V6);
			EL_ERROR(inet_pton(AF_INET6, cstr.get(), (char*)&ip.octet) <= 0, TException, TString::Format(U"%q cannot be parsed as IPv6 address", str));
			*this = ipaddr_t(ip);
		}
		else
		{
			EL_THROW(TException, TString::Format(U"%q is not a valid IPv4 or IPv6 address", str));
		}
	}

	/*********************************************************************************/

	TList<ipaddr_t> EnumMyIpAddresses()
	{
		TList<ipaddr_t> addrs;
		struct ifaddrs* ifaddr;
		EL_SYSERR(getifaddrs(&ifaddr));

		try
		{
			for(struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
			{
				if(ifa->ifa_addr == nullptr)
					continue;

				const int family = ifa->ifa_addr->sa_family;
				if (family == AF_INET || family == AF_INET6)
					addrs.Append(ConvertFromPosix(*ifa->ifa_addr).ip);
			}
		}
		catch(...)
		{
			freeifaddrs(ifaddr);
			throw;
		}

		freeifaddrs(ifaddr);
		return addrs;
	}

	TList<ipaddr_t> ResolveHostname(const text::string::TStringView hostname)
	{
		TList<ipaddr_t> addrs;

		struct addrinfo* res = nullptr;
		const int status = getaddrinfo(hostname.MakeCStr().get(), nullptr, nullptr, &res);
		EL_ERROR(status != 0, TException, gai_strerror(status));

		try
		{
			for(struct addrinfo* p = res; p != nullptr; p = p->ai_next)
				if(p->ai_family == AF_INET || p->ai_family == AF_INET6)
					addrs.Append(ConvertFromPosix(*p->ai_addr).ip);
		}
		catch(...)
		{
			freeaddrinfo(res);
			throw;
		}

		freeaddrinfo(res);

		for(usys_t i = 0; i < addrs.Count(); i++)
			for(usys_t j = i + 1; j < addrs.Count(); j++)
				if(addrs[i] == addrs[j])
				{
					addrs.Remove(j);
					j--;
				}

		return addrs;
	}

	/*********************************************************************************/

	handle_t TTcpClient::Handle()
	{
		return this->handle;
	}

	ipport_t TTcpClient::LocalAddress() const
	{
		return AddressFromSocket(this->handle);
	}

	ipport_t TTcpClient::RemoteAddress() const
	{
		return this->remote_address;
	}

	TTcpClient::TTcpClient(const io::text::string::TStringView remote_host, const port_t remote_port) :
		TTcpClient(remote_host, remote_port, -1)
	{
	}

	TTcpClient::TTcpClient(const io::text::string::TStringView remote_host, const port_t remote_port, const TTime connect_timeout) :
		TTcpClient(ResolveHostname(remote_host)[0], remote_port, connect_timeout)
	{
	}

	TTcpClient::TTcpClient(const ipaddr_t remote_ip, const port_t remote_port) :
		TTcpClient(remote_ip, remote_port, -1)
	{
	}

	TTcpClient::TTcpClient(const ipaddr_t remote_ip, const port_t remote_port, const TTime connect_timeout) :
		handle(CreateSocket(SOCK_STREAM, 0, remote_ip.Version())),
		remote_address{remote_ip, remote_port},
		on_rx_ready({ .read = true, .write = false, .other = false }, handle),
		on_tx_ready({ .read = false, .write = true, .other = false }, handle)
	{
		handle.BlockingIO(false);

		int result;
		if(remote_ip.IsV4())
		{
			auto addr = ConvertToPosixV4(remote_ip, remote_port);
			result = ::connect(this->handle, (const sockaddr*)&addr, sizeof(addr));
		}
		else
		{
			auto addr = ConvertToPosixV6(remote_ip, remote_port);
			result = ::connect(this->handle, (const sockaddr*)&addr, sizeof(addr));
		}

		if(result < 0)
		{
			EL_ERROR(errno != EINPROGRESS, TSyscallException, errno);
			EL_ERROR(!on_tx_ready.WaitFor(connect_timeout), TException, U"TCP connection attempt timed out");
			on_tx_ready.Reset();

			int socket_error = 0;
			socklen_t error_size = sizeof(socket_error);
			EL_SYSERR(::getsockopt(this->handle, SOL_SOCKET, SO_ERROR, &socket_error, &error_size));
			EL_ERROR(socket_error != 0, TSyscallException, socket_error);
		}
	}

	TTcpClient::TTcpClient(THandle handle, const ipport_t remote_address) :
		handle(std::move(handle)),
		remote_address(remote_address),
		on_rx_ready({ .read = true, .write = false, .other = false }, this->handle),
		on_tx_ready({ .read = false, .write = true, .other = false }, this->handle)
	{
	}

	usys_t TTcpClient::Read(byte_t* const arr_items, const usys_t n_items_max)
	{
		if(this->on_rx_ready.Handle() == -1)
			return 0;

		const ssize_t r = ::read(this->handle, arr_items, n_items_max);
		if(r > 0)
		{
			return r;
		}
		else if(r == 0)
		{
			// EOF
			CloseInput();
			return 0;
		}
		else // <0 (-1)
		{
			EL_ERROR(errno != EAGAIN && errno != EWOULDBLOCK, TSyscallException, errno);
			return 0;
		}
	}

	usys_t TTcpClient::Write(const byte_t* const arr_items, const usys_t n_items_max)
	{
		if(this->on_tx_ready.Handle() == -1)
			return 0;

		const ssize_t r = ::write(this->handle, arr_items, n_items_max);
		if(r > 0)
		{
			return r;
		}
		else if(r == 0)
		{
			CloseOutput();
			return 0;
		}
		else
		{
			EL_ERROR(errno != EAGAIN && errno != EWOULDBLOCK, TSyscallException, errno);
			return 0;
		}
	}

	const THandleWaitable* TTcpClient::OnInputReady() const
	{
		return this->on_rx_ready.Handle() >= 0 ? &this->on_rx_ready : nullptr;
	}

	const THandleWaitable* TTcpClient::OnOutputReady() const
	{
		return this->on_tx_ready.Handle() >= 0 ? &this->on_tx_ready : nullptr;
	}

	bool TTcpClient::CloseOutput()
	{
		if(this->on_tx_ready.Handle() != -1)
		{
			EL_SYSERR(shutdown(this->handle, SHUT_WR));
			this->on_tx_ready.Handle(-1);
		}

		return true;
	}

	bool TTcpClient::CloseInput()
	{
		if(this->on_rx_ready.Handle() != -1)
		{
			EL_SYSERR(shutdown(this->handle, SHUT_RD));
			this->on_rx_ready.Handle(-1);
		}

		return true;
	}

	void TTcpClient::Close()
	{
		this->handle.Close();
		this->on_rx_ready.Handle(-1);
		this->on_tx_ready.Handle(-1);
	}

	void TTcpClient::Flush()
	{
		// TCP has no userspace write buffer in this class. Successful write()
		// calls have already handed all accepted bytes to the kernel.
	}

	/*********************************************************************************/

	handle_t TTcpServer::Handle()
	{
		return this->handle;
	}

	const THandleWaitable& TTcpServer::OnClientConnect() const
	{
		return this->on_client_connect;
	}

	std::unique_ptr<IStreamClient> TTcpServer::AcceptStreamClient()
	{
		return AcceptClient();
	}

	std::unique_ptr<TTcpClient> TTcpServer::AcceptClient()
	{
		sockaddr_in6 addr = {};
		socklen_t len = sizeof(addr);
		const int r = accept4(this->handle, (sockaddr*)&addr, &len, SOCK_NONBLOCK | SOCK_CLOEXEC);

		if(r < 0)
		{
			EL_ERROR(errno != EAGAIN && errno != EWOULDBLOCK, TSyscallException, errno);
			return nullptr;
		}
		else
		{
			return New<TTcpClient>(THandle(r, true), ConvertFromPosix(addr));
		}
	}

	ipport_t TTcpServer::LocalAddress() const
	{
		return AddressFromSocket(this->handle);
	}

	TTcpServer::TTcpServer(const port_t port, const EIP version) : on_client_connect({ .read = true, .write = false, .other = false })
	{
		this->handle = CreateSocket(SOCK_STREAM | SOCK_NONBLOCK, port, version);
		EL_SYSERR(listen(this->handle, 64));
		on_client_connect.Handle(this->handle);
	}

	TTcpServer::TTcpServer(const ipaddr_t bind_ip, const port_t port) : on_client_connect({ .read = true, .write = false, .other = false })
	{
		this->handle = CreateSocket(SOCK_STREAM | SOCK_NONBLOCK, bind_ip, port);
		EL_SYSERR(listen(this->handle, 64));
		on_client_connect.Handle(this->handle);
	}

	/*********************************************************************************/

	handle_t TUdpSocket::Handle()
	{
		return this->handle;
	}

	ipport_t TUdpSocket::LocalAddress() const
	{
		return AddressFromSocket(this->handle);
	}

	const THandleWaitable& TUdpSocket::OnReceiveMsg() const
	{
		return on_rx_msg;
	}

	const THandleWaitable& TUdpSocket::OnTransmitReady() const
	{
		return on_tx_ready;
	}

	bool TUdpSocket::Receive(TList<byte_t>& msg_buffer, ipport_t& remote_address)
	{
		const ssize_t size = recvfrom(this->handle, nullptr, 0, MSG_TRUNC | MSG_PEEK, nullptr, nullptr);
		if(size < 0)
		{
			EL_ERROR(errno != EAGAIN && errno != EWOULDBLOCK, TSyscallException, errno);
			return false;
		}

		msg_buffer.SetCount((usys_t)size);

		sockaddr_storage addr = {};
		socklen_t addr_size = sizeof(addr);
		const ssize_t received = recvfrom(
			this->handle,
			size == 0 ? nullptr : msg_buffer.ItemPtr(0),
			msg_buffer.Count(),
			0,
			(sockaddr*)&addr,
			&addr_size
		);

		if(received < 0)
		{
			msg_buffer.Truncate();
			EL_ERROR(errno != EAGAIN && errno != EWOULDBLOCK, TSyscallException, errno);
			return false;
		}

		EL_ERROR(received != size, TLogicException);
		remote_address = ConvertFromPosix(*(const sockaddr*)&addr);
		return true;
	}

	bool TUdpSocket::Receive(TList<byte_t>& msg_buffer, ipaddr_t* const remote_ip, port_t* const remote_port)
	{
		ipport_t remote_address;
		if(!Receive(msg_buffer, remote_address))
			return false;

		if(remote_ip != nullptr)
			*remote_ip = remote_address.ip;
		if(remote_port != nullptr)
			*remote_port = remote_address.port;
		return true;
	}

	bool TUdpSocket::Receive(udp_datagram_t& datagram)
	{
		return Receive(datagram.data, datagram.source);
	}

	std::optional<udp_datagram_t> TUdpSocket::Receive()
	{
		udp_datagram_t datagram;
		if(!Receive(datagram))
			return std::nullopt;
		return std::move(datagram);
	}

	bool TUdpSocket::Send(const ipport_t remote_address, const array_t<const byte_t> msg_buffer)
	{
		ssize_t result = -1;
		switch(SocketDomain(this->handle))
		{
			case AF_INET:
			{
				EL_ERROR(!remote_address.ip.IsV4(), TInvalidArgumentException, "remote_address", "an IPv4 UDP socket cannot send to an IPv6 address");
				const sockaddr_in addr = ConvertToPosixV4(remote_address.ip, remote_address.port);
				result = sendto(this->handle, msg_buffer.ItemPtr(0), msg_buffer.Count(), 0, (const sockaddr*)&addr, sizeof(addr));
				break;
			}

			case AF_INET6:
			{
				// ipaddr_t stores IPv4 as IPv4-mapped IPv6, therefore the same
				// sockaddr_in6 representation works for native IPv6 and dual-stack IPv4.
				const sockaddr_in6 addr = ConvertToPosixV6(remote_address.ip, remote_address.port);
				result = sendto(this->handle, msg_buffer.ItemPtr(0), msg_buffer.Count(), 0, (const sockaddr*)&addr, sizeof(addr));
				break;
			}

			default:
				EL_THROW(TLogicException); // LCOV_EXCL_LINE
		}

		if(result < 0)
		{
			EL_ERROR(errno != EAGAIN && errno != EWOULDBLOCK, TSyscallException, errno);
			return false;
		}

		EL_ERROR(result != (ssys_t)msg_buffer.Count(), TException, TString::Format(U"UDP datagram truncated to %d bytes (out of %d bytes)", result, msg_buffer.Count()));
		return true;
	}

	bool TUdpSocket::Send(const ipport_t remote_address, const void* const buffer, const usys_t sz_buffer)
	{
		return Send(remote_address, array_t<const byte_t>::FromUnsafePointer((const byte_t*)buffer, sz_buffer));
	}

	bool TUdpSocket::Send(const ipaddr_t remote_ip, const port_t remote_port, const array_t<const byte_t> msg_buffer)
	{
		return Send(ipport_t{remote_ip, remote_port}, msg_buffer);
	}

	bool TUdpSocket::Send(const ipaddr_t remote_ip, const port_t remote_port, const void* const buffer, const usys_t sz_buffer)
	{
		return Send(ipport_t{remote_ip, remote_port}, buffer, sz_buffer);
	}

	bool TUdpSocket::Send(const TStringView remote_host, const port_t remote_port, const array_t<const byte_t> msg_buffer)
	{
		const int domain = SocketDomain(this->handle);
		for(const ipaddr_t& remote_ip : ResolveHostname(remote_host))
		{
			if(domain == AF_INET && !remote_ip.IsV4())
				continue;
			return Send(ipport_t{remote_ip, remote_port}, msg_buffer);
		}

		EL_THROW(TException, TString::Format(U"hostname %q did not resolve to an address compatible with this UDP socket", remote_host));
	}

	TUdpSocket::TUdpSocket(const port_t local_port, const EIP version) : on_rx_msg({ .read = true, .write = false, .other = false }), on_tx_ready({ .read = false, .write = true, .other = false })
	{
		this->handle = CreateSocket(SOCK_DGRAM | SOCK_NONBLOCK, local_port, version);
		on_rx_msg.Handle(handle);
		on_tx_ready.Handle(handle);
	}

	TUdpSocket::TUdpSocket(const ipaddr_t bind_ip, const port_t local_port) : on_rx_msg({ .read = true, .write = false, .other = false }), on_tx_ready({ .read = false, .write = true, .other = false })
	{
		this->handle = CreateSocket(SOCK_DGRAM | SOCK_NONBLOCK, bind_ip, local_port);
		on_rx_msg.Handle(handle);
		on_tx_ready.Handle(handle);
	}
}
