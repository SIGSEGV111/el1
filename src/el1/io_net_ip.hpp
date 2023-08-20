#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include "io_stream.hpp"
#include "io_collection_list.hpp"
#include "io_text_string.hpp"
#include "system_handle.hpp"
#include "system_waitable.hpp"

namespace el1::io::net::ip
{
	using namespace types;

	using port_t = u16_t; // port number in native ("host") byte order

	enum class EIP : u8_t
	{
		ANY = 0,
		V4 = 4,
		V6 = 6
	};

	// IPv6 address - can also store and handle IPv4 addresses
	struct ipaddr_t
	{
		union
		{
			byte_t octet[16]; // 01:23:45:67:89:1011:1213:1415
			u16_t group[8]; // big endian, 0:1:2:3:4:5:6:7
			u32_t u32[4];	// big endian
			u64_t u64[2];	// big endian
		};

		constexpr ipaddr_t& operator=(const ipaddr_t& rhs) = default;
		constexpr bool operator==(const ipaddr_t& rhs) const { return this->u64[0] == rhs.u64[0] && this->u64[1] == rhs.u64[1]; }
		constexpr bool operator!=(const ipaddr_t& rhs) const { return !(*this == rhs); }

		ipaddr_t& operator|=(const ipaddr_t& rhs);
		ipaddr_t& operator&=(const ipaddr_t& rhs);
		ipaddr_t operator|(const ipaddr_t& rhs) const EL_GETTER;
		ipaddr_t operator&(const ipaddr_t& rhs) const EL_GETTER;

		u32_t& IPv4();
		const u32_t& IPv4() const;

		EIP Version() const EL_GETTER;
		bool IsV4() const EL_GETTER { return Version() == EIP::V4; }
		bool IsV6() const EL_GETTER { return Version() == EIP::V6; }

		ipaddr_t(const u32_t ipv4);
		ipaddr_t(const u8_t cidr, const EIP ver); // MASK for given CIDR
		ipaddr_t(const text::string::TString&, const EIP version = EIP::ANY);

		constexpr ipaddr_t(const EIP version = EIP::V6) // ZERO
		{
			u64[0] = 0;
			u64[1] = 0;
			if(version == EIP::V4)
				group[5] = 0xffff;
		}

		constexpr ipaddr_t(ipaddr_t&&) = default;
		constexpr ipaddr_t(const ipaddr_t&) = default;

		explicit operator text::string::TString() const EL_GETTER;
	};

	struct ipport_t
	{
		ipaddr_t ip;
		port_t port;
	};

	io::collection::list::TList<ipaddr_t> ResolveHostname(const text::string::TString&);

	class TTcpClient : public stream::ISink<byte_t>, public stream::ISource<byte_t>
	{
		protected:
			system::handle::THandle handle;
			const ipport_t remote_address;
			system::waitable::THandleWaitable on_rx_ready;
			system::waitable::THandleWaitable on_tx_ready;

		public:
			system::handle::handle_t Handle() final override EL_GETTER;

			ipport_t LocalAddress() const EL_GETTER;
			ipport_t RemoteAddress() const EL_GETTER;

			TTcpClient(const io::text::string::TString remote_host, const port_t remote_port);
			TTcpClient(const ipaddr_t remote_ip, const port_t remote_port);
			TTcpClient(system::handle::THandle handle, const ipport_t remote_address);

			TTcpClient(TTcpClient&&) = default;
			TTcpClient(const TTcpClient&) = delete;

			usys_t Read(byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			usys_t Write(const byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;

			const system::waitable::THandleWaitable* OnInputReady() const final override;
			const system::waitable::THandleWaitable* OnOutputReady() const final override;

			bool CloseOutput() final override;
			bool CloseInput() final override;
			void Close() final override;
			void Flush() final override;
	};

	class TTcpServer
	{
		protected:
			system::handle::THandle handle;
			system::waitable::THandleWaitable on_client_connect;

		public:
			system::handle::handle_t Handle() EL_GETTER;

			const system::waitable::THandleWaitable& OnClientConnect() const EL_GETTER;
			std::unique_ptr<TTcpClient> AcceptClient(); // returns nullptr if no client was waiting

			ipport_t LocalAddress() const EL_GETTER;

			// if port == 0 => random free port
			TTcpServer(const port_t port = 0, const EIP version = EIP::ANY);
			TTcpServer(const ipaddr_t bind_ip, const port_t port);

			TTcpServer(TTcpServer&&) = default;
			TTcpServer(const TTcpServer&) = delete;
	};

	class TUdpNode
	{
		protected:
			system::handle::THandle handle;
			system::waitable::THandleWaitable on_rx_msg;
			system::waitable::THandleWaitable on_tx_ready;

		public:
			system::handle::handle_t Handle() EL_GETTER;

			ipport_t LocalAddress() const EL_GETTER;

			const system::waitable::THandleWaitable& OnReceiveMsg() const EL_GETTER;
			const system::waitable::THandleWaitable& OnTransmitReady() const EL_GETTER;

			// returns true if a message was received, false otherwise
			// if a message was received the msg_buffer will be resized to match the actual size of the message
			// and remote_ip and remote_port (if not null) will return the ip and port of the sender of the message
			bool Receive(collection::list::TList<byte_t>& msg_buffer, ipaddr_t* const remote_ip = nullptr, port_t* const remote_port = nullptr);

			// return true iof the full message was sent successfully, returns false if the outbound queue is full
			// throws an exception if the message was sent but truncated for any reason
			bool Send(const ipaddr_t remote_ip, const port_t remote_port, collection::list::array_t<const byte_t> msg_buffer) EL_WARN_UNUSED_RESULT;
			bool Send(const ipaddr_t remote_ip, const port_t remote_port, const void* const buffer, const usys_t sz_buffer) EL_WARN_UNUSED_RESULT;

			TUdpNode(const port_t local_port, const EIP version = EIP::ANY);
			TUdpNode(const ipaddr_t bind_ip, const port_t local_port);

			TUdpNode(TUdpNode&&) = default;
			TUdpNode(const TUdpNode&) = delete;
	};
}
