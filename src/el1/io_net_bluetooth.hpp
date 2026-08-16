#pragma once

#include "def.hpp"
#include "io_stream.hpp"
#include "io_text_string.hpp"
#include "io_types.hpp"
#include "system_handle.hpp"
#include "system_time.hpp"
#include "system_waitable.hpp"

namespace el1::io::net::bluetooth
{
	using namespace types;

	struct address_t
	{
		byte_t octet[6];

		address_t();
		explicit address_t(const text::string::TStringView address);
		explicit operator text::string::TString() const EL_GETTER;
		bool operator==(const address_t& rhs) const = default;
	};

#ifdef EL_OS_LINUX
	class TRfcommClient : public stream::IBinarySource, public stream::IBinarySink
	{
		private:
			system::handle::THandle handle;
			system::waitable::THandleWaitable on_rx_ready;
			system::waitable::THandleWaitable on_tx_ready;

		public:
			const address_t remote_address;
			const u8_t channel;

			system::handle::handle_t Handle() final override EL_GETTER;
			usys_t Read(byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			usys_t Write(const byte_t* const arr_items, const usys_t n_items_max) final override EL_WARN_UNUSED_RESULT;
			const system::waitable::THandleWaitable* OnInputReady() const final override;
			const system::waitable::THandleWaitable* OnOutputReady() const final override;
			bool CloseOutput() final override;
			bool CloseInput() final override;
			void Close() final override;
			void Flush() final override;

			TRfcommClient(const text::string::TStringView remote_address, const u8_t channel, const system::time::TTime connect_timeout = -1);
			TRfcommClient(const address_t remote_address, const u8_t channel, const system::time::TTime connect_timeout = -1);
			TRfcommClient(TRfcommClient&&) = default;
			TRfcommClient(const TRfcommClient&) = delete;
	};
#endif
}
