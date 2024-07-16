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

	static ipport_t AddressFromSocket(handle_t handle)
	{
		int domain = 0;
		socklen_t len = sizeof(domain);
		EL_SYSERR(getsockopt(handle, SOL_SOCKET, SO_DOMAIN, &domain, &len));

		switch(domain)
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
				EL_THROW(TException, "unknown address family");
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

	ipaddr_t::ipaddr_t(const TString& str, const EIP version)
	{
		const auto cstr = str.MakeCStr();

		if((version == EIP::ANY && str.Contains('.')) || version == EIP::V4)
		{
			ipaddr_t ip(EIP::V4);
			EL_ERROR(inet_pton(AF_INET, cstr.get(), (char*)&ip.IPv4()) <= 0, TException, TString::Format("%q cannot be parsed as IPv4 address", str));
			*this = ipaddr_t(ip);
		}
		else if((version == EIP::ANY && str.Contains(':')) || version == EIP::V6)
		{
			ipaddr_t ip(EIP::V6);
			EL_ERROR(inet_pton(AF_INET6, cstr.get(), (char*)&ip.octet) <= 0, TException, TString::Format("%q cannot be parsed as IPv6 address", str));
			*this = ipaddr_t(ip);
		}
		else
		{
			EL_THROW(TException, TString::Format("%q is not a valid IPv4 or IPv6 address", str));
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

	TList<ipaddr_t> ResolveHostname(const text::string::TString& hostname)
	{
		TList<ipaddr_t> addrs;

		struct addrinfo* res = nullptr;
		int status = 0;
		EL_ERROR(status = getaddrinfo(hostname.MakeCStr().get(), nullptr, nullptr, &res) != 0, TException, gai_strerror(status));

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

	// TTcpClient::TTcpClient(const io::text::string::TString remote_host, const port_t remote_port)
	// {
	// 	EL_NOT_IMPLEMENTED;
	// }

	TTcpClient::TTcpClient(const ipaddr_t remote_ip, const port_t remote_port) :
		handle(CreateSocket(SOCK_STREAM, 0, remote_ip.Version())),
		remote_address{remote_ip, remote_port},
		on_rx_ready({ .read = true, .write = false, .other = false }, handle),
		on_tx_ready({ .read = false, .write = true, .other = false }, handle)
	{
		if(remote_ip.IsV4())
		{
			auto addr = ConvertToPosixV4(remote_ip, remote_port);
			EL_SYSERR(connect(this->handle, (const sockaddr*)&addr, sizeof(addr)));
		}
		else
		{
			auto addr = ConvertToPosixV6(remote_ip, remote_port);
			EL_SYSERR(connect(this->handle, (const sockaddr*)&addr, sizeof(addr)));
		}

		const int flags = EL_SYSERR(fcntl(this->handle, F_GETFL));
		EL_SYSERR(fcntl(this->handle, F_SETFL, flags | O_NONBLOCK));
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
		// set and reset TCP_NODELAY
		EL_NOT_IMPLEMENTED;
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
			return std::unique_ptr<TTcpClient>(new TTcpClient(THandle(r, true), ConvertFromPosix(addr)));
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

	handle_t TUdpNode::Handle()
	{
		return this->handle;
	}

	ipport_t TUdpNode::LocalAddress() const
	{
		return AddressFromSocket(this->handle);
	}

	const THandleWaitable& TUdpNode::OnReceiveMsg() const
	{
		return on_rx_msg;
	}

	const THandleWaitable& TUdpNode::OnTransmitReady() const
	{
		return on_tx_ready;
	}

	bool TUdpNode::Receive(TList<byte_t>& msg_buffer, ipaddr_t* const remote_ip, port_t* const remote_port)
	{
		// get length of datagram
		const ssize_t r = recvfrom(this->handle, nullptr, 0, MSG_TRUNC | MSG_PEEK, nullptr, 0);

		if(r > (ssys_t)msg_buffer.Count())
		{
			msg_buffer.Inflate(r - msg_buffer.Count(), 0);
		}
		else if(r < 0)
		{
			EL_ERROR(errno != EAGAIN && errno != EWOULDBLOCK, TSyscallException, errno);
			return false;
		}
		else if(r < (ssys_t)msg_buffer.Count())
		{
			msg_buffer.Cut(0, msg_buffer.Count() - r);
		}

		struct sockaddr_in6 addr = {};
		socklen_t len = sizeof(addr);

		// get data of datagram
		EL_ERROR(r != recvfrom(this->handle, msg_buffer.ItemPtr(0), msg_buffer.Count(), 0, (struct sockaddr*)&addr, &len), TLogicException);

		const ipport_t ipp = ConvertFromPosix(addr);
		if(remote_ip != nullptr)
			*remote_ip = ipp.ip;
		if(remote_port != nullptr)
			*remote_port = ipp.port;

		return true;
	}

	bool TUdpNode::Send(const ipaddr_t remote_ip, const port_t remote_port, array_t<const byte_t> msg_buffer)
	{
		if(remote_ip.IsV4())
		{
			auto addr = ConvertToPosixV4(remote_ip, remote_port);
			const ssys_t r = sendto(this->handle, msg_buffer.ItemPtr(0), msg_buffer.Count(), 0, (const sockaddr*)&addr, sizeof(addr));
			if(r < 0)
			{
				EL_ERROR(errno != EAGAIN && errno != EWOULDBLOCK, TSyscallException, errno);
				return false;
			}

			EL_ERROR(r != (ssys_t)msg_buffer.Count(), TException, TString::Format("message truncated to %d bytes (out of %d bytes)", r, msg_buffer.Count()));
			return true;
		}
		else
		{
			auto addr = ConvertToPosixV6(remote_ip, remote_port);
			const ssys_t r = sendto(this->handle, msg_buffer.ItemPtr(0), msg_buffer.Count(), 0, (const sockaddr*)&addr, sizeof(addr));
			if(r < 0)
			{
				EL_ERROR(errno != EAGAIN && errno != EWOULDBLOCK, TSyscallException, errno);
				return false;
			}

			EL_ERROR(r != (ssys_t)msg_buffer.Count(), TException, TString::Format("message truncated to %d bytes (out of %d bytes)", r, msg_buffer.Count()));
			return true;
		}
	}

	bool TUdpNode::Send(const ipaddr_t remote_ip, const port_t remote_port, const void* const buffer, const usys_t sz_buffer)
	{
		return Send(remote_ip, remote_port, array_t<const byte_t>((const byte_t*)buffer, sz_buffer));
	}

	TUdpNode::TUdpNode(const port_t local_port, const EIP version) : on_rx_msg({ .read = true, .write = false, .other = false }), on_tx_ready({ .read = false, .write = true, .other = false })
	{
		this->handle = CreateSocket(SOCK_DGRAM | SOCK_NONBLOCK, local_port, version);
		on_rx_msg.Handle(handle);
		on_tx_ready.Handle(handle);
	}

	TUdpNode::TUdpNode(const ipaddr_t bind_ip, const port_t local_port) : on_rx_msg({ .read = true, .write = false, .other = false }), on_tx_ready({ .read = false, .write = true, .other = false })
	{
		this->handle = CreateSocket(SOCK_DGRAM | SOCK_NONBLOCK, bind_ip, local_port);
		on_rx_msg.Handle(handle);
		on_tx_ready.Handle(handle);
	}
}
