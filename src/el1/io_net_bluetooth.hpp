#pragma once

#include "def.hpp"
#include "io_stream.hpp"
#include "io_collection_list.hpp"
#include "io_text_string.hpp"
#include "io_types.hpp"
#include "system_handle.hpp"
#include "system_time.hpp"
#include "system_waitable.hpp"

#include <optional>
#include <utility>

namespace el1::io::net::bluetooth
{
	using namespace types;

	struct TUuid
	{
		byte_t octet[16];

		TUuid();
		explicit TUuid(const u16_t bluetooth_assigned_number);
		explicit TUuid(const u32_t bluetooth_assigned_number);
		explicit TUuid(const text::string::TStringView uuid);
		explicit operator text::string::TString() const EL_GETTER;
		bool operator==(const TUuid& rhs) const = default;
		bool operator!=(const TUuid& rhs) const = default;

		std::optional<u16_t> BluetoothAssignedNumber16() const EL_GETTER;
		std::optional<u32_t> BluetoothAssignedNumber32() const EL_GETTER;
	};

	inline const TUuid UUID_PUBLIC_BROWSE_GROUP(0x1002U);
	inline const TUuid UUID_PROTOCOL_L2CAP(0x0100U);
	inline const TUuid UUID_PROTOCOL_RFCOMM(0x0003U);
	inline const TUuid UUID_SERVICE_SERIAL_PORT(0x1101U);

	struct TProtocolDescriptor
	{
		const TUuid uuid;
		const io::collection::list::TList<u64_t> numeric_parameters;

		TProtocolDescriptor(TUuid uuid, io::collection::list::TList<u64_t> numeric_parameters) :
			uuid(std::move(uuid)),
			numeric_parameters(std::move(numeric_parameters))
		{
		}
	};

	struct TProfileDescriptor
	{
		const TUuid uuid;
		const u16_t version;

		TProfileDescriptor(TUuid uuid, const u16_t version) : uuid(std::move(uuid)), version(version) {}
	};

	struct TService
	{
		const u32_t handle;
		const text::string::TString name;
		const text::string::TString description;
		const text::string::TString provider;
		const io::collection::list::TList<TUuid> service_class_uuids;
		const io::collection::list::TList<TProfileDescriptor> profiles;
		const io::collection::list::TList<TProtocolDescriptor> protocols;

		TService(
			const u32_t handle,
			text::string::TString name,
			text::string::TString description,
			text::string::TString provider,
			io::collection::list::TList<TUuid> service_class_uuids,
			io::collection::list::TList<TProfileDescriptor> profiles,
			io::collection::list::TList<TProtocolDescriptor> protocols
		) :
			handle(handle),
			name(std::move(name)),
			description(std::move(description)),
			provider(std::move(provider)),
			service_class_uuids(std::move(service_class_uuids)),
			profiles(std::move(profiles)),
			protocols(std::move(protocols))
		{
		}

		bool HasServiceClass(const TUuid& uuid) const EL_GETTER;
		bool HasProfile(const TUuid& uuid) const EL_GETTER;
		bool HasProtocol(const TUuid& uuid) const EL_GETTER;
		const TProtocolDescriptor* FindProtocol(const TUuid& uuid) const EL_GETTER;
		std::optional<u8_t> RfcommChannel() const EL_GETTER;
		std::optional<u16_t> L2capPsm() const EL_GETTER;
	};

	enum class EServiceNameMatch : u8_t
	{
		ANY,
		EXACT,
		PREFIX,
		CONTAINS,
	};

	struct TServiceDiscoveryFilter
	{
		// UUIDs in this list are passed directly to the SDP service-search pattern.
		// SDP requires every UUID in the search pattern to be present in a matching record.
		io::collection::list::TList<TUuid> required_uuids;

		// The following filters are evaluated locally after SDP returns matching records.
		io::collection::list::TList<TUuid> service_class_uuids;
		io::collection::list::TList<TUuid> profile_uuids;
		io::collection::list::TList<TUuid> protocol_uuids;
		text::string::TString name;
		EServiceNameMatch name_match = EServiceNameMatch::ANY;

		bool Matches(const TService& service) const EL_GETTER;
	};

	struct TRfcommService
	{
		const text::string::TString name;
		const u8_t channel;

		TRfcommService(text::string::TString name, const u8_t channel) : name(std::move(name)), channel(channel) {}
	};

	struct address_t
	{
		byte_t octet[6];

		address_t();
		explicit address_t(const text::string::TStringView address);
		explicit operator text::string::TString() const EL_GETTER;
		bool operator==(const address_t& rhs) const = default;
	};

#ifdef EL_OS_LINUX
	io::collection::list::TList<TService> discoverServices(const address_t remote_address, const TServiceDiscoveryFilter& filter = {});

	// Compatibility wrapper for the old RFCOMM-only API.
	io::collection::list::TList<TRfcommService> discoverRfcommServices(const address_t remote_address, const u16_t service_class_id = 0x1101);

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
