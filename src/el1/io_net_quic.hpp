#pragma once

#include "def.hpp"
#include "error.hpp"
#include "io_net_ip.hpp"
#include "io_net_tls.hpp"
#include "io_collection_list.hpp"
#include "io_stream.hpp"
#include "io_text_string.hpp"

#include <memory>

namespace el1::io::net::quic
{
	using namespace io::types;

	struct server_config_t
	{
		tls::TPemSource certificate_chain;
		tls::TPemSource private_key;
		text::string::TString application_protocol = U"el1";
	};

	struct client_config_t
	{
		text::string::TString server_name;
		tls::TPemSource ca_certificates;
		text::string::TString application_protocol = U"el1";
		bool verify_peer = true;
	};

	struct TQuicException : error::IException
	{
		const text::string::TString msg;

		text::string::TString Message() const final override;
		error::IException* Clone() const override;

		explicit TQuicException(text::string::TString msg) : msg(std::move(msg)) {}
	};

	bool IsSupported() noexcept EL_GETTER;

	class TConnection;

	class TStream final : public stream::ISource<byte_t>, public stream::ISink<byte_t>
	{
		friend class TConnection;

		protected:
			struct data_t;
			std::unique_ptr<data_t> data;

			explicit TStream(void* const ssl, void* const connection_data);

		public:
			TStream(TStream&& rhs) noexcept;
			TStream(const TStream&) = delete;
			~TStream();

			u64_t Id() const EL_GETTER;
			bool IsLocal() const EL_GETTER;
			bool CanRead() const EL_GETTER;
			bool CanWrite() const EL_GETTER;

			usys_t Read(byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			const system::waitable::IWaitable* OnInputReady() const final override;
			usys_t Write(const byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			const system::waitable::IWaitable* OnOutputReady() const final override;
			using stream::ISink<byte_t>::WriteAll;
			void WriteAll(collection::list::array_t<const byte_t> data);

			bool CloseOutput() final override;
			void Reset(const u64_t application_error_code = 0);
	};

	class TConnection
	{
		friend class TClient;
		friend class TStream;
		friend class TServer;

		protected:
			struct data_t;
			std::unique_ptr<data_t> data;

			explicit TConnection(std::unique_ptr<data_t> data);

		public:
			TConnection(TConnection&&) = delete;
			TConnection(const TConnection&) = delete;
			virtual ~TConnection();

			ip::ipport_t LocalAddress() const EL_GETTER;
			text::string::TString ApplicationProtocol() const EL_GETTER;

			const system::waitable::IWaitable& OnStreamAvailable() const EL_GETTER;
			const system::waitable::IWaitable& OnStreamOpenReady(const bool unidirectional = false) const EL_GETTER;
			std::unique_ptr<TStream> OpenStream(const bool unidirectional = false);
			std::unique_ptr<TStream> TryOpenStream(const bool unidirectional = false);
			std::unique_ptr<TStream> AcceptStream();
			std::unique_ptr<TStream> TryAcceptStream();

			void Close(const u64_t application_error_code = 0, text::string::TStringView reason = U"");
	};

	class TClient final : public TConnection
	{
		protected:
			static std::unique_ptr<data_t> Connect(ip::ipport_t remote_address, client_config_t config);

		public:
			TClient(ip::ipport_t remote_address, client_config_t config = {});
			TClient(text::string::TStringView remote_host, ip::port_t remote_port, client_config_t config = {});
	};

	class TServer final
	{
		protected:
			struct data_t;
			std::unique_ptr<data_t> data;

		public:
			TServer(ip::TUdpSocket* const udp_socket, server_config_t config);
			TServer(TServer&&) = delete;
			TServer(const TServer&) = delete;
			~TServer();

			ip::ipport_t LocalAddress() const EL_GETTER;
			const system::waitable::IWaitable& OnConnectionAvailable() const EL_GETTER;
			std::unique_ptr<TConnection> AcceptConnection();
			std::unique_ptr<TConnection> TryAcceptConnection();
	};
}
