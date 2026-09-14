#include "io_net_bluetooth.hpp"
#ifdef EL_OS_LINUX

#include "error.hpp"
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <cerrno>
#include <cstdlib>
#include <sys/socket.h>
#include <unistd.h>
#include <array>

namespace el1::io::net::bluetooth
{
	using namespace error;
	using namespace system::handle;
	using namespace system::time;
	using namespace system::waitable;
	using namespace text::string;
	using namespace io::collection::list;


	namespace
	{
		TUuid fromBluezUuid(const uuid_t& source)
		{
			uuid_t uuid128 = {};
			switch(source.type)
			{
				case SDP_UUID16:
					::sdp_uuid16_to_uuid128(&uuid128, &source);
					break;
				case SDP_UUID32:
					::sdp_uuid32_to_uuid128(&uuid128, &source);
					break;
				case SDP_UUID128:
					uuid128 = source;
					break;
				default:
					EL_THROW(TInvalidArgumentException, "source", "unsupported Bluetooth UUID type");
			}

			TUuid result;
			for(usys_t i = 0; i < 16; i++)
				result.octet[i] = uuid128.value.uuid128.data[i];
			return result;
		}

		void toBluezUuid(const TUuid& source, uuid_t& destination)
		{
			const std::optional<u16_t> uuid16 = source.BluetoothAssignedNumber16();
			if(uuid16.has_value())
			{
				::sdp_uuid16_create(&destination, *uuid16);
				return;
			}

			const std::optional<u32_t> uuid32 = source.BluetoothAssignedNumber32();
			if(uuid32.has_value())
			{
				::sdp_uuid32_create(&destination, *uuid32);
				return;
			}

			::sdp_uuid128_create(&destination, source.octet);
		}

		TString getSdpString(const sdp_record_t* const record, int (*const getter)(const sdp_record_t*, char*, int))
		{
			std::array<char, 1024> buffer = {};
			if(getter(record, buffer.data(), static_cast<int>(buffer.size())) < 0)
				return TString();
			return TString(buffer.data());
		}

		TList<TUuid> getServiceClasses(const sdp_record_t* const record)
		{
			TList<TUuid> uuids;
			sdp_list_t* list = nullptr;
			if(::sdp_get_service_classes(record, &list) < 0)
				return uuids;

			for(sdp_list_t* item = list; item != nullptr; item = item->next)
			{
				const uuid_t* const uuid = static_cast<const uuid_t*>(item->data);
				if(uuid != nullptr)
					uuids.Append(fromBluezUuid(*uuid));
			}
			::sdp_list_free(list, ::free);
			return uuids;
		}

		TList<TProfileDescriptor> getProfiles(const sdp_record_t* const record)
		{
			TList<TProfileDescriptor> profiles;
			sdp_list_t* list = nullptr;
			if(::sdp_get_profile_descs(record, &list) < 0)
				return profiles;

			for(sdp_list_t* item = list; item != nullptr; item = item->next)
			{
				const sdp_profile_desc_t* const profile = static_cast<const sdp_profile_desc_t*>(item->data);
				if(profile != nullptr)
					profiles.Append(TProfileDescriptor(fromBluezUuid(profile->uuid), profile->version));
			}
			::sdp_list_free(list, ::free);
			return profiles;
		}

		void appendProtocolDescriptors(TList<TProtocolDescriptor>& protocols, sdp_list_t* const access_protocols)
		{
			for(sdp_list_t* sequence = access_protocols; sequence != nullptr; sequence = sequence->next)
			{
				for(sdp_list_t* descriptor = static_cast<sdp_list_t*>(sequence->data); descriptor != nullptr; descriptor = descriptor->next)
				{
					const sdp_data_t* data = static_cast<const sdp_data_t*>(descriptor->data);
					if(data == nullptr || (data->dtd != SDP_UUID16 && data->dtd != SDP_UUID32 && data->dtd != SDP_UUID128))
						continue;

					TList<u64_t> parameters;
					for(data = data->next; data != nullptr; data = data->next)
					{
						switch(data->dtd)
						{
							case SDP_UINT8:  parameters.Append(data->val.uint8);  break;
							case SDP_UINT16: parameters.Append(data->val.uint16); break;
							case SDP_UINT32: parameters.Append(data->val.uint32); break;
							case SDP_UINT64: parameters.Append(data->val.uint64); break;
							default: break;
						}
					}
					protocols.Append(TProtocolDescriptor(fromBluezUuid(static_cast<const sdp_data_t*>(descriptor->data)->val.uuid), std::move(parameters)));
				}
			}
		}

		TList<TProtocolDescriptor> getProtocols(const sdp_record_t* const record)
		{
			TList<TProtocolDescriptor> protocols;
			for(const bool additional : { false, true })
			{
				sdp_list_t* list = nullptr;
				const int result = additional ? ::sdp_get_add_access_protos(record, &list) : ::sdp_get_access_protos(record, &list);
				if(result < 0)
					continue;
				appendProtocolDescriptors(protocols, list);
				::sdp_list_foreach(list, reinterpret_cast<sdp_list_func_t>(::sdp_list_free), nullptr);
				::sdp_list_free(list, nullptr);
			}
			return protocols;
		}

		void appendSearchUuid(TList<uuid_t>& uuids, const TUuid& uuid)
		{
			uuid_t bluez_uuid = {};
			toBluezUuid(uuid, bluez_uuid);
			uuids.Append(bluez_uuid);
		}
	}

	TList<TService> discoverServices(const address_t remote_address, const TServiceDiscoveryFilter& filter)
	{
		bdaddr_t remote = {};
		for(usys_t i = 0; i < 6; i++)
			remote.b[i] = remote_address.octet[5 - i];

		bdaddr_t local = {};
		sdp_session_t* session = ::sdp_connect(&local, &remote, SDP_RETRY_IF_BUSY);
		EL_ERROR(session == nullptr, TSyscallException, errno);

		struct TSessionGuard
		{
			sdp_session_t* session;
			~TSessionGuard() { if(session != nullptr) ::sdp_close(session); }
		} session_guard { session };

		TList<uuid_t> search_uuids;
		for(const TUuid& uuid : filter.required_uuids)
			appendSearchUuid(search_uuids, uuid);
		for(const TUuid& uuid : filter.service_class_uuids)
			appendSearchUuid(search_uuids, uuid);
		for(const TUuid& uuid : filter.profile_uuids)
			appendSearchUuid(search_uuids, uuid);
		for(const TUuid& uuid : filter.protocol_uuids)
			appendSearchUuid(search_uuids, uuid);
		if(search_uuids.Count() == 0)
			appendSearchUuid(search_uuids, UUID_PUBLIC_BROWSE_GROUP);

		sdp_list_t* search_list = nullptr;
		for(uuid_t& uuid : search_uuids)
		{
			search_list = ::sdp_list_append(search_list, &uuid);
			EL_ERROR(search_list == nullptr, TOutOfMemoryException, sizeof(sdp_list_t));
		}

		uint32_t attribute_range = 0x0000ffff;
		sdp_list_t* attribute_list = ::sdp_list_append(nullptr, &attribute_range);
		if(attribute_list == nullptr)
		{
			::sdp_list_free(search_list, nullptr);
			EL_THROW(TOutOfMemoryException, sizeof(sdp_list_t));
		}

		sdp_list_t* response_list = nullptr;
		const int result = ::sdp_service_search_attr_req(session, search_list, SDP_ATTR_REQ_RANGE, attribute_list, &response_list);
		::sdp_list_free(attribute_list, nullptr);
		::sdp_list_free(search_list, nullptr);
		EL_ERROR(result < 0, TSyscallException, errno);

		TList<TService> services;
		for(sdp_list_t* item = response_list; item != nullptr; item = item->next)
		{
			const sdp_record_t* const record = static_cast<const sdp_record_t*>(item->data);
			if(record == nullptr)
				continue;

			TService service(
				record->handle,
				getSdpString(record, ::sdp_get_service_name),
				getSdpString(record, ::sdp_get_service_desc),
				getSdpString(record, ::sdp_get_provider_name),
				getServiceClasses(record),
				getProfiles(record),
				getProtocols(record)
			);
			if(filter.Matches(service))
				services.Append(std::move(service));
		}

		::sdp_list_free(response_list, reinterpret_cast<sdp_free_func_t>(::sdp_record_free));
		return services;
	}

	TList<TRfcommService> discoverRfcommServices(const address_t remote_address, const u16_t service_class_id)
	{
		TServiceDiscoveryFilter filter;
		filter.service_class_uuids.Append(TUuid(service_class_id));
		filter.protocol_uuids.Append(UUID_PROTOCOL_RFCOMM);

		const TList<TService> discovered = discoverServices(remote_address, filter);
		TList<TRfcommService> services;
		for(const TService& service : discovered)
		{
			const std::optional<u8_t> channel = service.RfcommChannel();
			if(channel.has_value())
				services.Append(TRfcommService(service.name, *channel));
		}
		return services;
	}

	TRfcommClient::TRfcommClient(const TStringView remote_address, const u8_t channel, const TTime connect_timeout) :
		TRfcommClient(address_t(remote_address), channel, connect_timeout)
	{
	}

	TRfcommClient::TRfcommClient(const address_t remote_address, const u8_t channel, const TTime connect_timeout) :
		handle(::socket(AF_BLUETOOTH, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, BTPROTO_RFCOMM), true),
		on_rx_ready({ .read = true, .write = false, .other = false }, handle),
		on_tx_ready({ .read = false, .write = true, .other = false }, handle),
		remote_address(remote_address),
		channel(channel)
	{
		EL_ERROR(handle == INVALID_HANDLE, TSyscallException, errno);
		EL_ERROR(channel == 0 || channel > 30, TInvalidArgumentException, "channel", "must be between 1 and 30");

		struct sockaddr_rc remote = {};
		remote.rc_family = AF_BLUETOOTH;
		remote.rc_channel = channel;
		for(usys_t i = 0; i < 6; i++)
			remote.rc_bdaddr.b[i] = remote_address.octet[5 - i];

		const int result = ::connect(handle, reinterpret_cast<const sockaddr*>(&remote), sizeof(remote));
		if(result < 0)
		{
			EL_ERROR(errno != EINPROGRESS, TSyscallException, errno);
			EL_ERROR(!on_tx_ready.WaitFor(connect_timeout), TException, U"RFCOMM connection attempt timed out");
			on_tx_ready.Reset();

			int socket_error = 0;
			socklen_t error_size = sizeof(socket_error);
			EL_SYSERR(::getsockopt(handle, SOL_SOCKET, SO_ERROR, &socket_error, &error_size));
			EL_ERROR(socket_error != 0, TSyscallException, socket_error);
		}
	}

	handle_t TRfcommClient::Handle()
	{
		return handle;
	}

	usys_t TRfcommClient::Read(byte_t* const arr_items, const usys_t n_items_max)
	{
		if(on_rx_ready.Handle() == INVALID_HANDLE)
			return 0;

		const ssize_t result = ::recv(handle, arr_items, n_items_max, 0);
		if(result > 0)
			return static_cast<usys_t>(result);
		if(result == 0)
		{
			CloseInput();
			return 0;
		}
		EL_ERROR(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR, TSyscallException, errno);
		return 0;
	}

	usys_t TRfcommClient::Write(const byte_t* const arr_items, const usys_t n_items_max)
	{
		if(on_tx_ready.Handle() == INVALID_HANDLE)
			return 0;

		const ssize_t result = ::send(handle, arr_items, n_items_max, MSG_NOSIGNAL);
		if(result > 0)
			return static_cast<usys_t>(result);
		if(result == 0)
		{
			CloseOutput();
			return 0;
		}
		EL_ERROR(errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR, TSyscallException, errno);
		return 0;
	}

	const THandleWaitable* TRfcommClient::OnInputReady() const
	{
		return on_rx_ready.Handle() == INVALID_HANDLE ? nullptr : &on_rx_ready;
	}

	const THandleWaitable* TRfcommClient::OnOutputReady() const
	{
		return on_tx_ready.Handle() == INVALID_HANDLE ? nullptr : &on_tx_ready;
	}

	bool TRfcommClient::CloseOutput()
	{
		if(on_tx_ready.Handle() != INVALID_HANDLE)
		{
			EL_SYSERR(::shutdown(handle, SHUT_WR));
			on_tx_ready.Handle(INVALID_HANDLE);
		}
		return true;
	}

	bool TRfcommClient::CloseInput()
	{
		if(on_rx_ready.Handle() != INVALID_HANDLE)
		{
			EL_SYSERR(::shutdown(handle, SHUT_RD));
			on_rx_ready.Handle(INVALID_HANDLE);
		}
		return true;
	}

	void TRfcommClient::Close()
	{
		handle.Close();
		on_rx_ready.Handle(INVALID_HANDLE);
		on_tx_ready.Handle(INVALID_HANDLE);
	}

	void TRfcommClient::Flush()
	{
	}
}

#endif
