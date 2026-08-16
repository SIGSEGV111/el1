#pragma once

#include "def.hpp"
#include "error.hpp"
#include "io_net_ip.hpp"
#include "io_file.hpp"
#include "io_collection_list.hpp"
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

	class TPemSource
	{
		public:
			enum class EType : u8_t
			{
				NONE,
				FILE,
				MEMORY,
			};

		private:
			EType type;
			file::TPath path;
			collection::list::TList<byte_t> data;

		public:
			EType Type() const noexcept EL_GETTER { return type; }
			bool IsEmpty() const noexcept EL_GETTER { return type == EType::NONE; }
			const file::TPath& Path() const EL_GETTER;
			collection::list::array_t<const byte_t> Data() const EL_LIFETIME_BOUND EL_GETTER;

			TPemSource();
			explicit TPemSource(file::TPath path);
			explicit TPemSource(collection::list::array_t<const byte_t> data);
			explicit TPemSource(collection::list::TList<byte_t> data);
	};

	struct server_config_t
	{
		TPemSource certificate_chain;
		TPemSource private_key;
		EVersion min_version = EVersion::TLS12;
	};

	struct client_config_t
	{
		text::string::TString server_name;
		TPemSource ca_certificates;
		EVersion min_version = EVersion::TLS12;
		bool verify_peer = true;
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
			TClient(text::string::TStringView remote_host, const ip::port_t remote_port, client_config_t config = {});
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
				file::TPath certificate_chain_file,
				file::TPath private_key_file,
				const EVersion min_version = EVersion::TLS12
			);
			TServer(TServer&&) = delete;
			TServer(const TServer&) = delete;
			~TServer() override;
	};
}
