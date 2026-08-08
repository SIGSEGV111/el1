#pragma once

#include "def.hpp"
#include "error.hpp"
#include "io_net_ip.hpp"
#include "io_stream.hpp"
#include "io_text_string.hpp"

#include <memory>

namespace el1::io::net::tls
{
	using namespace io::types;

	enum class EVersion : u8_t
	{
		TLS12 = 12,
		TLS13 = 13,
	};

	struct server_config_t
	{
		text::string::TString certificate_chain_file;
		text::string::TString private_key_file;
		EVersion min_version = EVersion::TLS12;
	};

	struct TTlsException : error::IException
	{
		const text::string::TString msg;

		text::string::TString Message() const final override;
		error::IException* Clone() const override;

		explicit TTlsException(text::string::TString msg) : msg(std::move(msg)) {}
	};

	class TClient final : public ip::IStreamClient
	{
		friend class TServer;

		protected:
			struct data_t;
			std::unique_ptr<data_t> data;

			TClient(void* const ssl_context, std::unique_ptr<ip::TTcpClient> tcp_client);

		public:
			TClient(TClient&&) noexcept;
			TClient(const TClient&) = delete;
			~TClient() override;

			system::handle::handle_t Handle() final override EL_GETTER;
			ip::ipport_t LocalAddress() const final override EL_GETTER;
			ip::ipport_t RemoteAddress() const final override EL_GETTER;

			usys_t Read(byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			usys_t Write(const byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;

			const system::waitable::IWaitable* OnInputReady() const final override;
			const system::waitable::IWaitable* OnOutputReady() const final override;

			bool CloseOutput() final override;
			bool CloseInput() final override;
			void Close() final override;
			void Flush() final override;
	};

	class TServer final : public ip::IStreamServer
	{
		protected:
			ip::TTcpServer* const tcp_server;
			void* ssl_context;

		public:
			const system::waitable::THandleWaitable& OnClientConnect() const final override EL_GETTER;
			std::unique_ptr<ip::IStreamClient> AcceptStreamClient() final override;
			std::unique_ptr<TClient> AcceptClient();
			ip::ipport_t LocalAddress() const EL_GETTER;

			TServer(ip::TTcpServer* const tcp_server, server_config_t config);
			TServer(
				ip::TTcpServer* const tcp_server,
				text::string::TString certificate_chain_file,
				text::string::TString private_key_file,
				const EVersion min_version = EVersion::TLS12
			);
			TServer(TServer&&) = delete;
			TServer(const TServer&) = delete;
			~TServer() override;
	};
}
