#include "io_net_tls.hpp"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509err.h>

#include <limits.h>
#include <string.h>

namespace el1::io::net::tls
{
	using namespace error;
	using namespace io::stream;
	using namespace io::text::string;
	using namespace system::waitable;

	TPemSource::TPemSource() : type(EType::NONE)
	{
	}

	TPemSource::TPemSource(file::TPath path) : type(EType::FILE), path(std::move(path))
	{
		EL_ERROR(this->path.IsEmpty(), TInvalidArgumentException, "path", "PEM file path must not be empty");
	}

	TPemSource::TPemSource(collection::list::array_t<const byte_t> data) : type(EType::MEMORY), data(data)
	{
		EL_ERROR(data.Count() == 0, TInvalidArgumentException, "data", "PEM data must not be empty");
	}

	TPemSource::TPemSource(collection::list::TList<byte_t> data) : type(EType::MEMORY), data(std::move(data))
	{
		EL_ERROR(this->data.Count() == 0, TInvalidArgumentException, "data", "PEM data must not be empty");
	}

	const file::TPath& TPemSource::Path() const
	{
		EL_ERROR(type != EType::FILE, TLogicException);
		return path;
	}

	collection::list::array_t<const byte_t> TPemSource::Data() const
	{
		EL_ERROR(type != EType::MEMORY, TLogicException);
		return data;
	}

	namespace
	{
		enum class EWaitDirection : u8_t
		{
			NONE,
			INPUT,
			OUTPUT,
		};

		static TString OpenSslError(const char* const context)
		{
			TString msg(context);
			bool first = true;

			for(unsigned long error_code = ERR_get_error(); error_code != 0; error_code = ERR_get_error())
			{
				char buffer[256];
				ERR_error_string_n(error_code, buffer, sizeof(buffer));
				msg += first ? ": " : "; ";
				msg += buffer;
				first = false;
			}

			if(first)
				msg += ": unknown OpenSSL error";

			return msg;
		}

		static int NativeVersion(const EVersion version)
		{
			switch(version)
			{
				case EVersion::TLS12: return TLS1_2_VERSION;
				case EVersion::TLS13: return TLS1_3_VERSION;
			}

			EL_THROW(TInvalidArgumentException, "version", "unsupported TLS version");
		}

		static EWaitDirection WaitDirectionFromSslError(SSL* const ssl, const int result, const char* const context)
		{
			const int ssl_error = SSL_get_error(ssl, result);
			switch(ssl_error)
			{
				case SSL_ERROR_WANT_READ:
					return EWaitDirection::INPUT;

				case SSL_ERROR_WANT_WRITE:
					return EWaitDirection::OUTPUT;

				case SSL_ERROR_ZERO_RETURN:
					return EWaitDirection::NONE;

				default:
					EL_THROW(TTlsException, OpenSslError(context));
			}
		}

		static std::unique_ptr<BIO, decltype(&BIO_free)> MakeMemoryBio(const TPemSource& source)
		{
			const auto data = source.Data();
			EL_ERROR(data.Count() > INT_MAX, TInvalidArgumentException, "data", "PEM data exceeds OpenSSL memory BIO size limit");
			BIO* const bio = BIO_new_mem_buf(data.ItemPtr(0), (int)data.Count());
			EL_ERROR(bio == nullptr, TTlsException, OpenSslError("failed to create PEM memory BIO"));
			return std::unique_ptr<BIO, decltype(&BIO_free)>(bio, BIO_free);
		}

		static void LoadCertificateChain(SSL_CTX* const context, const TPemSource& source)
		{
			if(source.Type() == TPemSource::EType::FILE)
			{
				auto filename = ((TString)source.Path()).MakeCStr();
				EL_ERROR(SSL_CTX_use_certificate_chain_file(context, filename.get()) != 1, TTlsException, OpenSslError("failed to load TLS certificate chain"));
				return;
			}

			auto bio = MakeMemoryBio(source);
			STACK_OF(X509_INFO)* const infos = PEM_X509_INFO_read_bio(bio.get(), nullptr, nullptr, nullptr);
			EL_ERROR(infos == nullptr, TTlsException, OpenSslError("failed to parse TLS certificate chain from memory"));

			usys_t n_certificates = 0;
			try
			{
				SSL_CTX_clear_chain_certs(context);
				for(int i = 0; i < sk_X509_INFO_num(infos); i++)
				{
					X509_INFO* const info = sk_X509_INFO_value(infos, i);
					if(info->x509 == nullptr)
						continue;

					if(n_certificates == 0)
						EL_ERROR(SSL_CTX_use_certificate(context, info->x509) != 1, TTlsException, OpenSslError("failed to use TLS certificate from memory"));
					else
						EL_ERROR(SSL_CTX_add1_chain_cert(context, info->x509) != 1, TTlsException, OpenSslError("failed to add TLS chain certificate from memory"));

					n_certificates++;
				}

				EL_ERROR(n_certificates == 0, TTlsException, TString("TLS certificate chain in memory contains no certificates"));
			}
			catch(...)
			{
				sk_X509_INFO_pop_free(infos, X509_INFO_free);
				throw;
			}

			sk_X509_INFO_pop_free(infos, X509_INFO_free);
		}

		static void LoadPrivateKey(SSL_CTX* const context, const TPemSource& source)
		{
			if(source.Type() == TPemSource::EType::FILE)
			{
				auto filename = ((TString)source.Path()).MakeCStr();
				EL_ERROR(SSL_CTX_use_PrivateKey_file(context, filename.get(), SSL_FILETYPE_PEM) != 1, TTlsException, OpenSslError("failed to load TLS private key"));
				return;
			}

			auto bio = MakeMemoryBio(source);
			EVP_PKEY* const key = PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr);
			EL_ERROR(key == nullptr, TTlsException, OpenSslError("failed to parse TLS private key from memory"));
			const int result = SSL_CTX_use_PrivateKey(context, key);
			EVP_PKEY_free(key);
			EL_ERROR(result != 1, TTlsException, OpenSslError("failed to use TLS private key from memory"));
		}

		static void AddCaCertificate(X509_STORE* const store, X509* const certificate)
		{
			ERR_clear_error();
			if(X509_STORE_add_cert(store, certificate) == 1)
				return;

			const unsigned long error_code = ERR_peek_last_error();
			if(ERR_GET_REASON(error_code) == X509_R_CERT_ALREADY_IN_HASH_TABLE)
			{
				ERR_clear_error();
				return;
			}

			EL_THROW(TTlsException, OpenSslError("failed to add CA certificate from memory"));
		}

		static void LoadCaCertificates(SSL_CTX* const context, const TPemSource& source)
		{
			if(source.Type() == TPemSource::EType::FILE)
			{
				auto filename = ((TString)source.Path()).MakeCStr();
				EL_ERROR(SSL_CTX_load_verify_locations(context, filename.get(), nullptr) != 1, TTlsException, OpenSslError("failed to load CA certificates"));
				return;
			}

			auto bio = MakeMemoryBio(source);
			STACK_OF(X509_INFO)* const infos = PEM_X509_INFO_read_bio(bio.get(), nullptr, nullptr, nullptr);
			EL_ERROR(infos == nullptr, TTlsException, OpenSslError("failed to parse CA certificates from memory"));

			usys_t n_certificates = 0;
			try
			{
				X509_STORE* const store = SSL_CTX_get_cert_store(context);
				EL_ERROR(store == nullptr, TTlsException, TString("TLS context has no certificate store"));

				for(int i = 0; i < sk_X509_INFO_num(infos); i++)
				{
					X509_INFO* const info = sk_X509_INFO_value(infos, i);
					if(info->x509 == nullptr)
						continue;

					AddCaCertificate(store, info->x509);
					n_certificates++;
				}

				EL_ERROR(n_certificates == 0, TTlsException, TString("CA PEM data in memory contains no certificates"));
			}
			catch(...)
			{
				sk_X509_INFO_pop_free(infos, X509_INFO_free);
				throw;
			}

			sk_X509_INFO_pop_free(infos, X509_INFO_free);
		}
	}

	struct TClient::data_t
	{
		static constexpr usys_t WRITE_BUFFER_SIZE = 16U * 1024U;

		struct TInputWaitable final : IWaitable
		{
			const data_t* const data;

			bool IsReady() const final override
			{
				return SSL_pending(data->ssl) > 0 || data->tcp_client->OnInputReady()->IsReady();
			}

			void Reset() const final override
			{
				data->tcp_client->OnInputReady()->Reset();
			}

			const THandleWaitable* HandleWaitable() const final override
			{
				return data->tcp_client->OnInputReady();
			}

			explicit TInputWaitable(const data_t* const data) : data(data) {}
		};

		std::unique_ptr<ip::TTcpClient> tcp_client;
		SSL_CTX* owned_ssl_context;
		SSL* ssl;
		const TInputWaitable on_input_ready;
		byte_t write_buffer[WRITE_BUFFER_SIZE];
		usys_t write_offset;
		usys_t write_count;
		EWaitDirection input_wait_direction;
		EWaitDirection output_wait_direction;
		bool input_closed;
		bool output_closed;
		bool closed;

		data_t(SSL_CTX* const ssl_context, std::unique_ptr<ip::TTcpClient> tcp_client, const bool server_mode, SSL_CTX* const owned_ssl_context = nullptr) :
			tcp_client(std::move(tcp_client)),
			owned_ssl_context(owned_ssl_context),
			ssl(nullptr),
			on_input_ready(this),
			write_offset(0),
			write_count(0),
			input_wait_direction(EWaitDirection::INPUT),
			output_wait_direction(EWaitDirection::OUTPUT),
			input_closed(false),
			output_closed(false),
			closed(false)
		{
			ERR_clear_error();
			ssl = SSL_new(static_cast<SSL_CTX*>(ssl_context));
			EL_ERROR(ssl == nullptr, TTlsException, OpenSslError("SSL_new() failed"));

			if(SSL_set_fd(ssl, this->tcp_client->Handle()) != 1)
			{
				const TString msg = OpenSslError("SSL_set_fd() failed");
				SSL_free(ssl);
				ssl = nullptr;
				EL_THROW(TTlsException, msg);
			}

			if(server_mode)
				SSL_set_accept_state(ssl);
			else
				SSL_set_connect_state(ssl);

			SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
		}

		~data_t()
		{
			if(ssl != nullptr)
				SSL_free(ssl);
			if(owned_ssl_context != nullptr)
				SSL_CTX_free(owned_ssl_context);
		}

		const IWaitable* Waitable(const EWaitDirection direction) const
		{
			switch(direction)
			{
				case EWaitDirection::INPUT:
					return &on_input_ready;

				case EWaitDirection::OUTPUT:
					return tcp_client->OnOutputReady();

				case EWaitDirection::NONE:
					return nullptr;
			}

			EL_THROW(TLogicException);
		}

		bool DrivePendingWrite()
		{
			if(write_count == 0)
				return true;

			ERR_clear_error();
			size_t n_written = 0;
			const int result = SSL_write_ex(ssl, write_buffer + write_offset, write_count, &n_written);
			if(result == 1)
			{
				EL_ERROR(n_written == 0 || n_written > write_count, TLogicException);
				write_offset += n_written;
				write_count -= n_written;
				output_wait_direction = EWaitDirection::OUTPUT;

				if(write_count == 0)
					write_offset = 0;

				return write_count == 0;
			}

			output_wait_direction = WaitDirectionFromSslError(ssl, result, "SSL_write_ex() failed");
			if(output_wait_direction == EWaitDirection::NONE)
			{
				output_closed = true;
				write_offset = 0;
				write_count = 0;
				return false;
			}

			return false;
		}
	};

	TString TTlsException::Message() const
	{
		return msg;
	}

	IException* TTlsException::Clone() const
	{
		return new TTlsException(*this);
	}

	TClient::TClient(void* const ssl_context, std::unique_ptr<ip::TTcpClient> tcp_client) :
		data(new data_t(static_cast<SSL_CTX*>(ssl_context), std::move(tcp_client), true))
	{
	}

	TClient::TClient(TString remote_host, const ip::port_t remote_port, client_config_t config)
	{
		if(config.server_name.Length() == 0)
			config.server_name = remote_host;

		ERR_clear_error();
		SSL_CTX* const context = SSL_CTX_new(TLS_client_method());
		EL_ERROR(context == nullptr, TTlsException, OpenSslError("SSL_CTX_new() failed"));

		try
		{
			EL_ERROR(SSL_CTX_set_min_proto_version(context, NativeVersion(config.min_version)) != 1, TTlsException, OpenSslError("SSL_CTX_set_min_proto_version() failed"));
			SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION);

			if(config.verify_peer)
			{
				SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);

				if(config.ca_certificates.IsEmpty())
					EL_ERROR(SSL_CTX_set_default_verify_paths(context) != 1, TTlsException, OpenSslError("failed to load default CA paths"));
				else
					LoadCaCertificates(context, config.ca_certificates);
			}
			else
			{
				SSL_CTX_set_verify(context, SSL_VERIFY_NONE, nullptr);
			}

			auto tcp_client = New<ip::TTcpClient>(remote_host, remote_port);
			data.reset(new data_t(context, std::move(tcp_client), false, context));

			auto server_name = config.server_name.MakeCStr();
			EL_ERROR(SSL_set_tlsext_host_name(data->ssl, server_name.get()) != 1, TTlsException, OpenSslError("failed to set TLS SNI hostname"));
			if(config.verify_peer)
				EL_ERROR(SSL_set1_host(data->ssl, server_name.get()) != 1, TTlsException, OpenSslError("failed to configure TLS hostname verification"));
		}
		catch(...)
		{
			if(data == nullptr)
				SSL_CTX_free(context);
			throw;
		}
	}

	TClient::TClient(TClient&&) noexcept = default;
	TClient::~TClient() = default;

	system::handle::handle_t TClient::Handle()
	{
		// The TCP file descriptor carries encrypted TLS records, not the plaintext
		// represented by this stream. Returning it would allow optimizations such
		// as sendfile() to bypass TLS entirely.
		return system::handle::INVALID_HANDLE;
	}

	ip::ipport_t TClient::LocalAddress() const
	{
		return data->tcp_client->LocalAddress();
	}

	ip::ipport_t TClient::RemoteAddress() const
	{
		return data->tcp_client->RemoteAddress();
	}

	usys_t TClient::Read(byte_t* const arr_items, const usys_t n_items_max)
	{
		if(data->closed || data->input_closed || n_items_max == 0)
			return 0;

		ERR_clear_error();
		size_t n_read = 0;
		const int result = SSL_read_ex(data->ssl, arr_items, n_items_max, &n_read);
		if(result == 1)
		{
			data->input_wait_direction = EWaitDirection::INPUT;
			return n_read;
		}

		data->input_wait_direction = WaitDirectionFromSslError(data->ssl, result, "SSL_read_ex() failed");
		if(data->input_wait_direction == EWaitDirection::NONE)
			data->input_closed = true;

		return 0;
	}

	usys_t TClient::Write(const byte_t* const arr_items, const usys_t n_items_max)
	{
		if(data->closed || data->output_closed || n_items_max == 0)
			return 0;

		if(data->write_count != 0)
		{
			data->DrivePendingWrite();
			if(data->write_count != 0 || data->output_closed)
				return 0;
		}

		const usys_t n_buffer = util::Min(n_items_max, data_t::WRITE_BUFFER_SIZE);
		memcpy(data->write_buffer, arr_items, n_buffer);
		data->write_offset = 0;
		data->write_count = n_buffer;
		data->DrivePendingWrite();
		return n_buffer;
	}

	const IWaitable* TClient::OnInputReady() const
	{
		if(data->closed || data->input_closed)
			return nullptr;

		return data->Waitable(data->input_wait_direction);
	}

	const IWaitable* TClient::OnOutputReady() const
	{
		if(data->closed || data->output_closed)
			return nullptr;

		return data->Waitable(data->output_wait_direction);
	}

	void TClient::Flush()
	{
		if(data->closed || data->output_closed)
			return;

		while(data->write_count != 0)
		{
			data->DrivePendingWrite();
			if(data->write_count != 0)
			{
				const IWaitable* const waitable = OnOutputReady();
				EL_ERROR(waitable == nullptr, TSinkFloodedException);
				waitable->WaitFor();
			}
		}

	}

	bool TClient::CloseOutput()
	{
		if(data->closed || data->output_closed)
			return true;

		Flush();

		for(;;)
		{
			ERR_clear_error();
			const int result = SSL_shutdown(data->ssl);
			if(result >= 0)
				break;

			const EWaitDirection direction = WaitDirectionFromSslError(data->ssl, result, "SSL_shutdown() failed");
			const IWaitable* const waitable = data->Waitable(direction);
			EL_ERROR(waitable == nullptr, TTlsException, "TLS transport closed during shutdown");
			waitable->WaitFor();
		}

		// A successful first SSL_shutdown() sends close_notify. Waiting for the
		// peer's close_notify would turn this half-close operation into an
		// unbounded blocking read; the HTTP server closes the connection anyway.
		data->output_closed = true;
		return data->tcp_client->CloseOutput();
	}

	bool TClient::CloseInput()
	{
		if(data->closed || data->input_closed)
			return true;

		data->input_closed = true;
		return data->tcp_client->CloseInput();
	}

	void TClient::Close()
	{
		if(data->closed)
			return;

		data->closed = true;
		data->input_closed = true;
		data->output_closed = true;
		data->write_count = 0;
		data->tcp_client->Close();
	}

	const THandleWaitable& TServer::OnClientConnect() const
	{
		return tcp_server->OnClientConnect();
	}

	std::unique_ptr<ip::IStreamClient> TServer::AcceptStreamClient()
	{
		return AcceptClient();
	}

	std::unique_ptr<TClient> TServer::AcceptClient()
	{
		std::unique_ptr<ip::TTcpClient> tcp_client = tcp_server->AcceptClient();
		if(tcp_client == nullptr)
			return nullptr;

		return std::unique_ptr<TClient>(new TClient(ssl_context, std::move(tcp_client)));
	}

	ip::ipport_t TServer::LocalAddress() const
	{
		return tcp_server->LocalAddress();
	}

	TServer::TServer(ip::TTcpServer* const tcp_server, server_config_t config) :
		tcp_server(tcp_server),
		ssl_context(nullptr)
	{
		EL_ERROR(tcp_server == nullptr, TInvalidArgumentException, "tcp_server", "tcp_server must not be null");
		EL_ERROR(config.certificate_chain.IsEmpty(), TInvalidArgumentException, "certificate_chain", "certificate chain source must not be empty");
		EL_ERROR(config.private_key.IsEmpty(), TInvalidArgumentException, "private_key", "private key source must not be empty");

		ERR_clear_error();
		SSL_CTX* const context = SSL_CTX_new(TLS_server_method());
		EL_ERROR(context == nullptr, TTlsException, OpenSslError("SSL_CTX_new() failed"));
		ssl_context = context;

		try
		{
			EL_ERROR(SSL_CTX_set_min_proto_version(context, NativeVersion(config.min_version)) != 1, TTlsException, OpenSslError("SSL_CTX_set_min_proto_version() failed"));
			SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION);

			LoadCertificateChain(context, config.certificate_chain);
			LoadPrivateKey(context, config.private_key);
			EL_ERROR(SSL_CTX_check_private_key(context) != 1, TTlsException, OpenSslError("TLS private key does not match certificate"));
		}
		catch(...)
		{
			SSL_CTX_free(context);
			ssl_context = nullptr;
			throw;
		}
	}

	TServer::TServer(
		ip::TTcpServer* const tcp_server,
		file::TPath certificate_chain_file,
		file::TPath private_key_file,
		const EVersion min_version
	) :
		TServer(tcp_server, server_config_t{
			.certificate_chain = TPemSource(std::move(certificate_chain_file)),
			.private_key = TPemSource(std::move(private_key_file)),
			.min_version = min_version,
		})
	{
	}

	TServer::~TServer()
	{
		if(ssl_context != nullptr)
			SSL_CTX_free(static_cast<SSL_CTX*>(ssl_context));
	}
}
