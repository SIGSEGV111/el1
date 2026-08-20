#include "io_net_quic.hpp"

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509err.h>

#include <arpa/inet.h>
#include <limits.h>
#include <string.h>

#include "system_time_timer.hpp"

namespace el1::io::net::quic
{
	using namespace error;
	using namespace io::collection::list;
	using namespace io::text::string;
	using namespace system::waitable;
	using namespace system::time;
	using namespace system::time::timer;

	namespace
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		static TString OpenSslError(const char* const context)
		{
			TString msg(context);
			bool first = true;

			for(unsigned long error_code = ERR_get_error(); error_code != 0; error_code = ERR_get_error())
			{
				char buffer[256];
				ERR_error_string_n(error_code, buffer, sizeof(buffer));
				msg += first ? TStringView(U": ") : TStringView(U"; ");
				msg += TString(buffer);
				first = false;
			}

			if(first)
				msg += TStringView(U": unknown OpenSSL error");

			return msg;
		}

		static std::unique_ptr<BIO, decltype(&BIO_free)> MakeMemoryBio(const tls::TPemSource& source)
		{
			const auto data = source.Data();
			EL_ERROR(data.Count() > INT_MAX, TInvalidArgumentException, "data", "PEM data exceeds OpenSSL memory BIO size limit");
			BIO* const bio = BIO_new_mem_buf(data.ItemPtr(0), (int)data.Count());
			EL_ERROR(bio == nullptr, TQuicException, OpenSslError("failed to create PEM memory BIO"));
			return std::unique_ptr<BIO, decltype(&BIO_free)>(bio, BIO_free);
		}

		static void LoadCertificateChain(SSL_CTX* const context, const tls::TPemSource& source)
		{
			EL_ERROR(source.IsEmpty(), TInvalidArgumentException, "certificate_chain", "QUIC server certificate chain is required");
			if(source.Type() == tls::TPemSource::EType::FILE)
			{
				auto filename = ((TString)source.Path()).MakeCStr();
				EL_ERROR(SSL_CTX_use_certificate_chain_file(context, filename.get()) != 1, TQuicException, OpenSslError("failed to load QUIC certificate chain"));
				return;
			}

			auto bio = MakeMemoryBio(source);
			STACK_OF(X509_INFO)* const infos = PEM_X509_INFO_read_bio(bio.get(), nullptr, nullptr, nullptr);
			EL_ERROR(infos == nullptr, TQuicException, OpenSslError("failed to parse QUIC certificate chain from memory"));

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
						EL_ERROR(SSL_CTX_use_certificate(context, info->x509) != 1, TQuicException, OpenSslError("failed to use QUIC certificate from memory"));
					else
						EL_ERROR(SSL_CTX_add1_chain_cert(context, info->x509) != 1, TQuicException, OpenSslError("failed to add QUIC chain certificate from memory"));
					n_certificates++;
				}
				EL_ERROR(n_certificates == 0, TQuicException, U"QUIC certificate chain in memory contains no certificates");
			}
			catch(...)
			{
				sk_X509_INFO_pop_free(infos, X509_INFO_free);
				throw;
			}
			sk_X509_INFO_pop_free(infos, X509_INFO_free);
		}

		static void LoadPrivateKey(SSL_CTX* const context, const tls::TPemSource& source)
		{
			EL_ERROR(source.IsEmpty(), TInvalidArgumentException, "private_key", "QUIC server private key is required");
			if(source.Type() == tls::TPemSource::EType::FILE)
			{
				auto filename = ((TString)source.Path()).MakeCStr();
				EL_ERROR(SSL_CTX_use_PrivateKey_file(context, filename.get(), SSL_FILETYPE_PEM) != 1, TQuicException, OpenSslError("failed to load QUIC private key"));
				return;
			}

			auto bio = MakeMemoryBio(source);
			EVP_PKEY* const key = PEM_read_bio_PrivateKey(bio.get(), nullptr, nullptr, nullptr);
			EL_ERROR(key == nullptr, TQuicException, OpenSslError("failed to parse QUIC private key from memory"));
			const int result = SSL_CTX_use_PrivateKey(context, key);
			EVP_PKEY_free(key);
			EL_ERROR(result != 1, TQuicException, OpenSslError("failed to use QUIC private key"));
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
			EL_THROW(TQuicException, OpenSslError("failed to add QUIC CA certificate from memory"));
		}

		static void LoadCaCertificates(SSL_CTX* const context, const tls::TPemSource& source)
		{
			if(source.Type() == tls::TPemSource::EType::FILE)
			{
				auto filename = ((TString)source.Path()).MakeCStr();
				EL_ERROR(SSL_CTX_load_verify_locations(context, filename.get(), nullptr) != 1, TQuicException, OpenSslError("failed to load QUIC CA certificates"));
				return;
			}

			auto bio = MakeMemoryBio(source);
			STACK_OF(X509_INFO)* const infos = PEM_X509_INFO_read_bio(bio.get(), nullptr, nullptr, nullptr);
			EL_ERROR(infos == nullptr, TQuicException, OpenSslError("failed to parse QUIC CA certificates from memory"));
			usys_t n_certificates = 0;
			try
			{
				X509_STORE* const store = SSL_CTX_get_cert_store(context);
				EL_ERROR(store == nullptr, TQuicException, U"QUIC TLS context has no certificate store");
				for(int i = 0; i < sk_X509_INFO_num(infos); i++)
				{
					X509_INFO* const info = sk_X509_INFO_value(infos, i);
					if(info->x509 == nullptr)
						continue;
					AddCaCertificate(store, info->x509);
					n_certificates++;
				}
				EL_ERROR(n_certificates == 0, TQuicException, U"QUIC CA PEM data in memory contains no certificates");
			}
			catch(...)
			{
				sk_X509_INFO_pop_free(infos, X509_INFO_free);
				throw;
			}
			sk_X509_INFO_pop_free(infos, X509_INFO_free);
		}

		static TList<byte_t> AlpnWire(const TStringView protocol)
		{
			EL_ERROR(protocol.IsEmpty(), TInvalidArgumentException, "application_protocol", "QUIC requires a non-empty ALPN application protocol");
			const TString str(protocol);
			auto cstr = str.MakeCStr();
			const usys_t size = strlen(cstr.get());
			EL_ERROR(size == 0 || size > 255, TInvalidArgumentException, "application_protocol", "QUIC ALPN protocol must encode to 1..255 UTF-8 bytes");
			TList<byte_t> wire;
			wire.Append((byte_t)size);
			for(usys_t i = 0; i < size; i++)
				wire.Append((byte_t)cstr.get()[i]);
			return wire;
		}

		static int SelectAlpn(SSL*, const unsigned char** out, unsigned char* outlen, const unsigned char* in, unsigned int inlen, void* arg)
		{
			const TList<byte_t>* const expected = reinterpret_cast<const TList<byte_t>*>(arg);
			const usys_t expected_size = expected->Count() - 1;
			const byte_t* const expected_data = expected->ItemPtr(1);
			for(unsigned int p = 0; p < inlen; )
			{
				const unsigned int size = in[p++];
				if(size > inlen - p)
					return SSL_TLSEXT_ERR_ALERT_FATAL;
				if(size == expected_size && memcmp(in + p, expected_data, size) == 0)
				{
					*out = in + p;
					*outlen = (unsigned char)size;
					return SSL_TLSEXT_ERR_OK;
				}
				p += size;
			}
			return SSL_TLSEXT_ERR_ALERT_FATAL;
		}

		static std::unique_ptr<BIO_ADDR, decltype(&BIO_ADDR_free)> BioAddress(const ip::ipport_t address)
		{
			BIO_ADDR* const result = BIO_ADDR_new();
			EL_ERROR(result == nullptr, TQuicException, OpenSslError("failed to allocate QUIC peer address"));
			const int family = address.ip.IsV4() ? AF_INET : AF_INET6;
			const void* const raw = address.ip.IsV4() ? (const void*)&address.ip.IPv4() : (const void*)address.ip.octet;
			const size_t size = address.ip.IsV4() ? 4U : 16U;
			EL_ERROR(BIO_ADDR_rawmake(result, family, raw, size, htons(address.port)) != 1, TQuicException, OpenSslError("failed to create QUIC peer address"));
			return std::unique_ptr<BIO_ADDR, decltype(&BIO_ADDR_free)>(result, BIO_ADDR_free);
		}

		static TString SelectedAlpn(SSL* const ssl)
		{
			const unsigned char* data = nullptr;
			unsigned int size = 0;
			SSL_get0_alpn_selected(ssl, &data, &size);
			EL_ERROR(data == nullptr || size == 0, TQuicException, U"QUIC connection has no negotiated ALPN protocol");
			return TString((const char*)data, size);
		}


		class TQuicWaitable final : public IWaitable
		{
			SSL* const connection;
			SSL* const target;
			ip::TUdpSocket* const socket;
			const u64_t events;
			mutable const THandleWaitable* handle_waitables[3];
			mutable std::unique_ptr<TTimeWaitable> timer;

			void UpdateTimer() const
			{
				struct timeval timeout = {};
				int infinite = 0;
				EL_ERROR(SSL_get_event_timeout(connection, &timeout, &infinite) != 1, TQuicException, OpenSslError("failed to query QUIC event timeout"));
				if(infinite)
				{
					timer.reset();
					return;
				}
				const TTime delay((double)timeout.tv_sec + (double)timeout.tv_usec / 1000000.0);
				timer = New<TTimeWaitable>(EClock::MONOTONIC, TTime::Now(EClock::MONOTONIC) + delay);
			}

			bool PollTarget() const
			{
				if(events == 0)
					return false;
				SSL_POLL_ITEM item = { .desc = SSL_as_poll_descriptor(target), .events = events, .revents = 0 };
				const struct timeval timeout = {};
				size_t result_count = 0;
				ERR_clear_error();
				EL_ERROR(SSL_poll(&item, 1, sizeof(item), &timeout, SSL_POLL_FLAG_NO_HANDLE_EVENTS, &result_count) != 1, TQuicException, OpenSslError("failed to poll QUIC state"));
				return result_count != 0 && item.revents != 0;
			}

			bool DriveEvents() const
			{
				const bool network_ready = socket->OnReceiveMsg().IsReady() || socket->OnTransmitReady().IsReady();
				const bool timer_ready = timer != nullptr && timer->IsReady();
				if(!network_ready && !timer_ready)
					return false;
				socket->OnReceiveMsg().Reset();
				socket->OnTransmitReady().Reset();
				ERR_clear_error();
				EL_ERROR(SSL_handle_events(connection) != 1, TQuicException, OpenSslError("failed to handle QUIC events"));
				UpdateTimer();
				return true;
			}

			public:
				TQuicWaitable(SSL* const connection, SSL* const target, ip::TUdpSocket* const socket, const u64_t events) : connection(connection), target(target), socket(socket), events(events)
				{
					UpdateTimer();
				}

				bool IsReady() const final override
				{
					if(PollTarget())
						return true;
					const bool drove_events = DriveEvents();
					return drove_events || PollTarget();
				}

				void Reset() const final override
				{
					socket->OnReceiveMsg().Reset();
					socket->OnTransmitReady().Reset();
					UpdateTimer();
				}

				io::collection::array::array_t<const THandleWaitable*> HandleWaitables() const final override EL_GETTER
				{
					usys_t count = 0;
					if(SSL_net_read_desired(connection))
						handle_waitables[count++] = &socket->OnReceiveMsg();
					if(SSL_net_write_desired(connection))
						handle_waitables[count++] = &socket->OnTransmitReady();
					if(timer != nullptr)
					{
						const io::collection::array::array_t<const THandleWaitable*> timer_waitables = timer->HandleWaitables();
						if(!timer_waitables.IsEmpty())
							handle_waitables[count++] = timer_waitables[0];
					}
					return io::collection::array::array_t<const THandleWaitable*>::FromUnsafePointer(handle_waitables, count);
				}
		};

		class TQuicListenerWaitable final : public IWaitable
		{
			SSL* const listener;
			ip::TUdpSocket* const socket;

			bool PollListener() const
			{
				SSL_POLL_ITEM item = { .desc = SSL_as_poll_descriptor(listener), .events = SSL_POLL_EVENT_IC | SSL_POLL_EVENT_EL, .revents = 0 };
				const struct timeval timeout = {};
				size_t result_count = 0;
				ERR_clear_error();
				EL_ERROR(SSL_poll(&item, 1, sizeof(item), &timeout, SSL_POLL_FLAG_NO_HANDLE_EVENTS, &result_count) != 1, TQuicException, OpenSslError("failed to poll QUIC listener state"));
				return result_count != 0 && item.revents != 0;
			}

			public:
				TQuicListenerWaitable(SSL* const listener, ip::TUdpSocket* const socket) : listener(listener), socket(socket) {}

				bool IsReady() const final override
				{
					return PollListener() || socket->OnReceiveMsg().IsReady();
				}

				void Reset() const final override
				{
					socket->OnReceiveMsg().Reset();
				}

				io::collection::array::array_t<const THandleWaitable*> HandleWaitables() const final override EL_GETTER
				{
					return socket->OnReceiveMsg().HandleWaitables();
				}
		};

		static void ConfigureClientContext(SSL_CTX* const context, const client_config_t& config)
		{
			if(config.verify_peer)
			{
				SSL_CTX_set_verify(context, SSL_VERIFY_PEER, nullptr);
				if(config.ca_certificates.IsEmpty())
					EL_ERROR(SSL_CTX_set_default_verify_paths(context) != 1, TQuicException, OpenSslError("failed to load default QUIC CA paths"));
				else
					LoadCaCertificates(context, config.ca_certificates);
			}
			else
			{
				SSL_CTX_set_verify(context, SSL_VERIFY_NONE, nullptr);
			}
		}
#endif
	}

	TString TQuicException::Message() const
	{
		return msg;
	}

	IException* TQuicException::Clone() const
	{
		return new TQuicException(*this);
	}

	bool IsSupported() noexcept
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		return true;
#else
		return false;
#endif
	}

	struct TConnection::data_t
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		SSL* ssl = nullptr;
		std::unique_ptr<TQuicWaitable> on_events;
		std::unique_ptr<TQuicWaitable> on_stream_available;
		std::unique_ptr<TQuicWaitable> on_bidi_stream_open_ready;
		std::unique_ptr<TQuicWaitable> on_uni_stream_open_ready;
#endif
		std::unique_ptr<ip::TUdpSocket> owned_socket;
		ip::TUdpSocket* udp_socket = nullptr;
		bool closed = false;

#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		void InitWaitables()
		{
			on_events = New<TQuicWaitable>(ssl, ssl, udp_socket, 0);
			on_stream_available = New<TQuicWaitable>(ssl, ssl, udp_socket, SSL_POLL_EVENT_ISE);
			on_bidi_stream_open_ready = New<TQuicWaitable>(ssl, ssl, udp_socket, SSL_POLL_EVENT_OSB | SSL_POLL_EVENT_EC);
			on_uni_stream_open_ready = New<TQuicWaitable>(ssl, ssl, udp_socket, SSL_POLL_EVENT_OSU | SSL_POLL_EVENT_EC);
		}
#endif

#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		void CompleteHandshake(const bool server)
		{
			for(;;)
			{
				ERR_clear_error();
				const int result = server ? SSL_accept(ssl) : SSL_connect(ssl);
				if(result == 1)
					return;
				const int ssl_error = SSL_get_error(ssl, result);
				if(ssl_error != SSL_ERROR_WANT_READ && ssl_error != SSL_ERROR_WANT_WRITE)
					EL_THROW(TQuicException, OpenSslError(server ? "QUIC server handshake failed" : "QUIC handshake failed"));
				on_events->WaitFor();
			}
		}
#endif

		~data_t()
		{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
			on_uni_stream_open_ready.reset();
			on_bidi_stream_open_ready.reset();
			on_stream_available.reset();
			on_events.reset();
			if(ssl != nullptr)
				SSL_free(ssl);
#endif
		}
	};

	struct TStream::data_t
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		SSL* ssl = nullptr;
		TConnection::data_t* connection = nullptr;
		std::unique_ptr<TQuicWaitable> on_input_ready;
		std::unique_ptr<TQuicWaitable> on_output_ready;
		bool input_closed = false;
		bool output_closed = false;
#endif

		~data_t()
		{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
			on_input_ready.reset();
			on_output_ready.reset();
			if(ssl != nullptr)
				SSL_free(ssl);
#endif
		}
	};


	TStream::TStream(void* const ssl, void* const connection_data) : data(New<data_t>())
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		data->ssl = (SSL*)ssl;
		data->connection = (TConnection::data_t*)connection_data;
		data->on_input_ready = New<TQuicWaitable>(data->connection->ssl, data->ssl, data->connection->udp_socket, SSL_POLL_EVENT_RE);
		data->on_output_ready = New<TQuicWaitable>(data->connection->ssl, data->ssl, data->connection->udp_socket, SSL_POLL_EVENT_WE);
#else
		(void)ssl;
		(void)connection_data;
#endif
	}

	TStream::TStream(TStream&& rhs) noexcept = default;
	TStream::~TStream() = default;

	u64_t TStream::Id() const
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		return SSL_get_stream_id(data->ssl);
#else
		return 0;
#endif
	}

	bool TStream::IsLocal() const
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		return SSL_is_stream_local(data->ssl) == 1;
#else
		return false;
#endif
	}

	bool TStream::CanRead() const
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		return (SSL_get_stream_type(data->ssl) & SSL_STREAM_TYPE_READ) != 0;
#else
		return false;
#endif
	}

	bool TStream::CanWrite() const
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		return (SSL_get_stream_type(data->ssl) & SSL_STREAM_TYPE_WRITE) != 0;
#else
		return false;
#endif
	}

	usys_t TStream::Read(byte_t* const arr_items, const usys_t n_items_max)
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		EL_ERROR(data == nullptr || data->ssl == nullptr, TLogicException);
		if(n_items_max == 0)
			return 0;
		EL_ERROR(!CanRead(), TQuicException, U"QUIC stream is not readable");
		size_t read = 0;
		ERR_clear_error();
		const int result = SSL_read_ex(data->ssl, arr_items, n_items_max, &read);
		if(result == 1)
			return read;
		const int ssl_error = SSL_get_error(data->ssl, result);
		if(ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE)
			return 0;
		if(ssl_error == SSL_ERROR_ZERO_RETURN)
		{
			data->input_closed = true;
			return 0;
		}
		EL_THROW(TQuicException, OpenSslError("failed to read QUIC stream"));
#else
		(void)arr_items;
		(void)n_items_max;
		EL_THROW(TQuicException, U"QUIC requires OpenSSL 3.5 or newer");
#endif
	}

	const IWaitable* TStream::OnInputReady() const
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		if(data == nullptr || data->input_closed || !CanRead())
			return nullptr;
		return data->on_input_ready.get();
#else
		return nullptr;
#endif
	}

	usys_t TStream::Write(const byte_t* const arr_items, const usys_t n_items_max)
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		EL_ERROR(data == nullptr || data->ssl == nullptr, TLogicException);
		if(n_items_max == 0)
			return 0;
		EL_ERROR(!CanWrite(), TQuicException, U"QUIC stream is not writable");
		size_t written = 0;
		ERR_clear_error();
		const int result = SSL_write_ex(data->ssl, arr_items, n_items_max, &written);
		if(result == 1)
			return written;
		const int ssl_error = SSL_get_error(data->ssl, result);
		if(ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE)
			return 0;
		EL_THROW(TQuicException, OpenSslError("failed to write QUIC stream"));
#else
		(void)arr_items;
		(void)n_items_max;
		EL_THROW(TQuicException, U"QUIC requires OpenSSL 3.5 or newer");
#endif
	}

	const IWaitable* TStream::OnOutputReady() const
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		if(data == nullptr || data->output_closed || !CanWrite())
			return nullptr;
		return data->on_output_ready.get();
#else
		return nullptr;
#endif
	}

	void TStream::WriteAll(const array_t<const byte_t> input)
	{
		usys_t offset = 0;
		while(offset < input.Count())
		{
			const usys_t written = Write(input.ItemPtr(offset), input.Count() - offset);
			if(written != 0)
			{
				offset += written;
				continue;
			}
			const IWaitable* const waitable = OnOutputReady();
			EL_ERROR(waitable == nullptr, TQuicException, U"QUIC stream output is closed");
			waitable->WaitFor();
		}
	}

	bool TStream::CloseOutput()
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		EL_ERROR(data == nullptr || data->ssl == nullptr, TLogicException);
		if(!CanWrite())
			return false;
		for(;;)
		{
			ERR_clear_error();
			const int result = SSL_stream_conclude(data->ssl, 0);
			if(result == 1)
			{
				data->output_closed = true;
				return true;
			}
			const int ssl_error = SSL_get_error(data->ssl, result);
			if(ssl_error != SSL_ERROR_WANT_READ && ssl_error != SSL_ERROR_WANT_WRITE)
				EL_THROW(TQuicException, OpenSslError("failed to conclude QUIC stream"));
			data->on_output_ready->WaitFor();
		}
#else
		EL_THROW(TQuicException, U"QUIC requires OpenSSL 3.5 or newer");
#endif
	}

	void TStream::Reset(const u64_t application_error_code)
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		SSL_STREAM_RESET_ARGS args = { .quic_error_code = application_error_code };
		EL_ERROR(SSL_stream_reset(data->ssl, &args, sizeof(args)) != 1, TQuicException, OpenSslError("failed to reset QUIC stream"));
#else
		(void)application_error_code;
		EL_THROW(TQuicException, U"QUIC requires OpenSSL 3.5 or newer");
#endif
	}

	TConnection::TConnection(std::unique_ptr<data_t> data) : data(std::move(data))
	{
	}

	TConnection::~TConnection() = default;

	ip::ipport_t TConnection::LocalAddress() const
	{
		EL_ERROR(data == nullptr || data->udp_socket == nullptr, TLogicException);
		return data->udp_socket->LocalAddress();
	}

	TString TConnection::ApplicationProtocol() const
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		EL_ERROR(data == nullptr || data->ssl == nullptr, TLogicException);
		return SelectedAlpn(data->ssl);
#else
		EL_THROW(TQuicException, U"QUIC requires OpenSSL 3.5 or newer");
#endif
	}

	const IWaitable& TConnection::OnStreamAvailable() const
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		EL_ERROR(data == nullptr || data->on_stream_available == nullptr, TLogicException);
		return *data->on_stream_available;
#else
		EL_THROW(TQuicException, U"QUIC requires OpenSSL 3.5 or newer");
#endif
	}

	const IWaitable& TConnection::OnStreamOpenReady(const bool unidirectional) const
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		EL_ERROR(data == nullptr || data->ssl == nullptr || data->closed, TLogicException);
		return unidirectional ? *data->on_uni_stream_open_ready : *data->on_bidi_stream_open_ready;
#else
		(void)unidirectional;
		EL_THROW(TQuicException, U"QUIC requires OpenSSL 3.5 or newer");
#endif
	}

	std::unique_ptr<TStream> TConnection::OpenStream(const bool unidirectional)
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		for(;;)
		{
			auto stream = TryOpenStream(unidirectional);
			if(stream != nullptr)
				return stream;
			OnStreamOpenReady(unidirectional).WaitFor();
		}
#else
		(void)unidirectional;
		EL_THROW(TQuicException, U"QUIC requires OpenSSL 3.5 or newer");
#endif
	}

	std::unique_ptr<TStream> TConnection::TryOpenStream(const bool unidirectional)
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		EL_ERROR(data == nullptr || data->ssl == nullptr || data->closed, TLogicException);
		ERR_clear_error();
		SSL* const stream = SSL_new_stream(data->ssl, (unidirectional ? SSL_STREAM_FLAG_UNI : 0) | SSL_STREAM_FLAG_NO_BLOCK);
		if(stream != nullptr)
			return std::unique_ptr<TStream>(new TStream(stream, data.get()));
		const unsigned long error = ERR_peek_error();
		if(error != 0 && ERR_GET_REASON(error) == SSL_R_STREAM_COUNT_LIMITED)
		{
			ERR_clear_error();
			return nullptr;
		}
		EL_THROW(TQuicException, OpenSslError("failed to open QUIC stream"));
#else
		(void)unidirectional;
		EL_THROW(TQuicException, U"QUIC requires OpenSSL 3.5 or newer");
#endif
	}

	std::unique_ptr<TStream> TConnection::AcceptStream()
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		for(;;)
		{
			auto stream = TryAcceptStream();
			if(stream != nullptr)
				return stream;
			OnStreamAvailable().WaitFor();
		}
#else
		EL_THROW(TQuicException, U"QUIC requires OpenSSL 3.5 or newer");
#endif
	}

	std::unique_ptr<TStream> TConnection::TryAcceptStream()
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		EL_ERROR(data == nullptr || data->ssl == nullptr || data->closed, TLogicException);
		ERR_clear_error();
		SSL* const stream = SSL_accept_stream(data->ssl, SSL_ACCEPT_STREAM_NO_BLOCK);
		if(stream != nullptr)
			return std::unique_ptr<TStream>(new TStream(stream, data.get()));
		if(ERR_peek_error() == 0)
			return nullptr;
		EL_THROW(TQuicException, OpenSslError("failed to accept QUIC stream"));
#else
		EL_THROW(TQuicException, U"QUIC requires OpenSSL 3.5 or newer");
#endif
	}

	void TConnection::Close(const u64_t application_error_code, const TStringView reason)
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		if(data == nullptr || data->ssl == nullptr || data->closed)
			return;
		const TString reason_string(reason);
		auto reason_cstr = reason_string.MakeCStr();
		const SSL_SHUTDOWN_EX_ARGS args = {
			.quic_error_code = application_error_code,
			.quic_reason = reason.IsEmpty() ? nullptr : reason_cstr.get(),
		};
		for(;;)
		{
			ERR_clear_error();
			const int result = SSL_shutdown_ex(data->ssl, SSL_SHUTDOWN_FLAG_WAIT_PEER, &args, sizeof(args));
			if(result == 1)
				break;
			const int ssl_error = SSL_get_error(data->ssl, result);
			if(ssl_error != SSL_ERROR_WANT_READ && ssl_error != SSL_ERROR_WANT_WRITE)
				EL_THROW(TQuicException, OpenSslError("failed to close QUIC connection"));
			data->on_events->WaitFor();
		}
		data->closed = true;
#else
		(void)application_error_code;
		(void)reason;
		EL_THROW(TQuicException, U"QUIC requires OpenSSL 3.5 or newer");
#endif
	}

	std::unique_ptr<TConnection::data_t> TClient::Connect(const ip::ipport_t remote_address, client_config_t config)
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		EL_ERROR(config.verify_peer && config.server_name.Length() == 0, TInvalidArgumentException, "server_name", "QUIC peer verification requires a server name");
		TList<byte_t> alpn = AlpnWire(config.application_protocol);

		SSL_CTX* context = SSL_CTX_new(OSSL_QUIC_client_method());
		EL_ERROR(context == nullptr, TQuicException, OpenSslError("failed to create QUIC client context"));
		try
		{
			ConfigureClientContext(context, config);
			SSL* const ssl = SSL_new(context);
			EL_ERROR(ssl == nullptr, TQuicException, OpenSslError("failed to create QUIC client connection"));
			SSL_CTX_free(context);
			context = nullptr;

			auto result = std::make_unique<data_t>();
			result->ssl = ssl;
			result->owned_socket = std::make_unique<ip::TUdpSocket>(0U, remote_address.ip.Version());
			result->udp_socket = result->owned_socket.get();

			EL_ERROR(SSL_set_fd(ssl, result->udp_socket->Handle()) != 1, TQuicException, OpenSslError("failed to bind UDP socket to QUIC client"));
			EL_ERROR(SSL_set_blocking_mode(ssl, 0) != 1, TQuicException, OpenSslError("failed to enable nonblocking QUIC client mode"));
			auto peer = BioAddress(remote_address);
			EL_ERROR(SSL_set1_initial_peer_addr(ssl, peer.get()) != 1, TQuicException, OpenSslError("failed to set initial QUIC peer address"));
			EL_ERROR(SSL_set_default_stream_mode(ssl, SSL_DEFAULT_STREAM_MODE_NONE) != 1, TQuicException, OpenSslError("failed to disable implicit QUIC stream"));
			EL_ERROR(SSL_set_alpn_protos(ssl, alpn.ItemPtr(0), (unsigned int)alpn.Count()) != 0, TQuicException, OpenSslError("failed to configure QUIC ALPN"));

			if(config.server_name.Length() != 0)
			{
				auto server_name = config.server_name.MakeCStr();
				EL_ERROR(SSL_set_tlsext_host_name(ssl, server_name.get()) != 1, TQuicException, OpenSslError("failed to set QUIC SNI"));
				if(config.verify_peer)
					EL_ERROR(SSL_set1_host(ssl, server_name.get()) != 1, TQuicException, OpenSslError("failed to set QUIC certificate hostname"));
			}

			result->InitWaitables();
			result->CompleteHandshake(false);
			EL_ERROR(SSL_get_verify_result(ssl) != X509_V_OK && config.verify_peer, TQuicException, TString::Format(U"QUIC certificate verification failed: %s", X509_verify_cert_error_string(SSL_get_verify_result(ssl))));
			(void)SelectedAlpn(ssl);
			return result;
		}
		catch(...)
		{
			if(context != nullptr)
				SSL_CTX_free(context);
			throw;
		}
#else
		(void)remote_address;
		(void)config;
		EL_THROW(TQuicException, U"QUIC requires OpenSSL 3.5 or newer");
#endif
	}

	TClient::TClient(const ip::ipport_t remote_address, client_config_t config) : TConnection(Connect(remote_address, std::move(config)))
	{
	}

	TClient::TClient(const TStringView remote_host, const ip::port_t remote_port, client_config_t config) : TConnection([&]() -> std::unique_ptr<data_t>
	{
		if(config.server_name.Length() == 0)
			config.server_name = remote_host;
		const auto addresses = ip::ResolveHostname(remote_host);
		EL_ERROR(addresses.Count() == 0, TQuicException, TString::Format(U"could not resolve QUIC host %q", remote_host));
		return Connect({ addresses[0], remote_port }, std::move(config));
	}())
	{
	}

	struct TServer::data_t
	{
		ip::TUdpSocket* udp_socket = nullptr;
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		SSL_CTX* context = nullptr;
		SSL* listener = nullptr;
		std::unique_ptr<TQuicListenerWaitable> on_connection_available;
		TList<byte_t> alpn;
#endif

		~data_t()
		{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
			on_connection_available.reset();
			if(listener != nullptr)
				SSL_free(listener);
			if(context != nullptr)
				SSL_CTX_free(context);
#endif
		}
	};

	TServer::TServer(ip::TUdpSocket* const udp_socket, server_config_t config) : data(std::make_unique<data_t>())
	{
		EL_ERROR(udp_socket == nullptr, TInvalidArgumentException, "udp_socket", "QUIC server requires a UDP socket");
		data->udp_socket = udp_socket;
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		data->alpn = AlpnWire(config.application_protocol);
		data->context = SSL_CTX_new(OSSL_QUIC_server_method());
		EL_ERROR(data->context == nullptr, TQuicException, OpenSslError("failed to create QUIC server context"));
		LoadCertificateChain(data->context, config.certificate_chain);
		LoadPrivateKey(data->context, config.private_key);
		EL_ERROR(SSL_CTX_check_private_key(data->context) != 1, TQuicException, OpenSslError("QUIC certificate and private key do not match"));
		SSL_CTX_set_alpn_select_cb(data->context, SelectAlpn, &data->alpn);

		data->listener = SSL_new_listener(data->context, 0);
		EL_ERROR(data->listener == nullptr, TQuicException, OpenSslError("failed to create QUIC listener"));
		EL_ERROR(SSL_set_fd(data->listener, udp_socket->Handle()) != 1, TQuicException, OpenSslError("failed to bind UDP socket to QUIC listener"));
		EL_ERROR(SSL_set_blocking_mode(data->listener, 0) != 1, TQuicException, OpenSslError("failed to enable nonblocking QUIC listener mode"));
		EL_ERROR(SSL_listen(data->listener) != 1, TQuicException, OpenSslError("failed to start QUIC listener"));
		data->on_connection_available = New<TQuicListenerWaitable>(data->listener, data->udp_socket);
#else
		(void)config;
		EL_THROW(TQuicException, U"QUIC server support requires OpenSSL 3.5 or newer");
#endif
	}

	TServer::~TServer() = default;

	ip::ipport_t TServer::LocalAddress() const
	{
		return data->udp_socket->LocalAddress();
	}

	const IWaitable& TServer::OnConnectionAvailable() const
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		EL_ERROR(data == nullptr || data->on_connection_available == nullptr, TLogicException);
		return *data->on_connection_available;
#else
		EL_THROW(TQuicException, U"QUIC server support requires OpenSSL 3.5 or newer");
#endif
	}

	std::unique_ptr<TConnection> TServer::AcceptConnection()
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		for(;;)
		{
			auto connection = TryAcceptConnection();
			if(connection != nullptr)
				return connection;
			OnConnectionAvailable().WaitFor();
		}
#else
		EL_THROW(TQuicException, U"QUIC server support requires OpenSSL 3.5 or newer");
#endif
	}

	std::unique_ptr<TConnection> TServer::TryAcceptConnection()
	{
#if OPENSSL_VERSION_NUMBER >= 0x30500000L && !defined(OPENSSL_NO_QUIC)
		ERR_clear_error();
		SSL* const ssl = SSL_accept_connection(data->listener, SSL_ACCEPT_CONNECTION_NO_BLOCK);
		if(ssl == nullptr)
		{
			if(ERR_peek_error() == 0)
				return nullptr;
			EL_THROW(TQuicException, OpenSslError("failed to accept QUIC connection"));
		}
		std::unique_ptr<SSL, decltype(&SSL_free)> ssl_guard(ssl, SSL_free);
		EL_ERROR(SSL_set_blocking_mode(ssl, 0) != 1, TQuicException, OpenSslError("failed to enable nonblocking QUIC connection mode"));
		EL_ERROR(SSL_set_default_stream_mode(ssl, SSL_DEFAULT_STREAM_MODE_NONE) != 1, TQuicException, OpenSslError("failed to disable implicit QUIC stream"));
		auto connection = std::make_unique<TConnection::data_t>();
		connection->ssl = ssl;
		ssl_guard.release();
		connection->udp_socket = data->udp_socket;
		connection->InitWaitables();
		connection->CompleteHandshake(true);
		(void)SelectedAlpn(ssl);
		return std::unique_ptr<TConnection>(new TConnection(std::move(connection)));
#else
		EL_THROW(TQuicException, U"QUIC server support requires OpenSSL 3.5 or newer");
#endif
	}
}
