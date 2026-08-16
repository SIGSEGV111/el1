#include "security_gssapi.hpp"

#include <gssapi/gssapi.h>

namespace el1::security::gssapi
{
	using namespace el1::error;

	struct TInitiatorContext::data_t
	{
		gss_name_t target_name = GSS_C_NO_NAME;
		gss_ctx_id_t context = GSS_C_NO_CONTEXT;
	};

	static TString StatusText(const OM_uint32 status, const int type)
	{
		OM_uint32 minor = 0;
		OM_uint32 context = 0;
		TString result;
		do
		{
			gss_buffer_desc buffer = GSS_C_EMPTY_BUFFER;
			const OM_uint32 major = gss_display_status(&minor, status, type, GSS_C_NO_OID, &context, &buffer);
			if(GSS_ERROR(major))
				break;
			if(result.Length() > 0)
				result += TStringView(U"; ");
			if(buffer.length > 0)
				result += TString((const char*)buffer.value, buffer.length);
			gss_release_buffer(&minor, &buffer);
		}
		while(context != 0);
		return result;
	}

	TString TGssapiException::Message() const
	{
		return message;
	}

	IException* TGssapiException::Clone() const
	{
		return new TGssapiException(*this);
	}

	TGssapiException::TGssapiException(const u32_t major_status, const u32_t minor_status) :
		major_status(major_status),
		minor_status(minor_status),
		message(StatusText(major_status, GSS_C_GSS_CODE) + TString(U": ") + StatusText(minor_status, GSS_C_MECH_CODE))
	{
	}

	TInitiatorContext::TInitiatorContext(const TString& service_name) :
		data(new data_t())
	{
		const auto service_cstr = service_name.MakeCStr();
		gss_buffer_desc buffer = { strlen(service_cstr.get()), service_cstr.get() };
		OM_uint32 minor = 0;
		const OM_uint32 major = gss_import_name(&minor, &buffer, GSS_C_NT_HOSTBASED_SERVICE, &data->target_name);
		if(GSS_ERROR(major))
		{
			delete data;
			data = nullptr;
			EL_THROW(TGssapiException, major, minor);
		}
	}

	TInitiatorContext::~TInitiatorContext()
	{
		if(data == nullptr)
			return;
		OM_uint32 minor = 0;
		if(data->context != GSS_C_NO_CONTEXT)
			gss_delete_sec_context(&minor, &data->context, GSS_C_NO_BUFFER);
		if(data->target_name != GSS_C_NO_NAME)
			gss_release_name(&minor, &data->target_name);
		delete data;
	}

	TInitiatorStep TInitiatorContext::Process(const array_t<const byte_t> input_token)
	{
		gss_buffer_desc input_buffer = GSS_C_EMPTY_BUFFER;
		if(input_token.Count() > 0)
		{
			input_buffer.value = const_cast<byte_t*>(input_token.ItemPtr(0));
			input_buffer.length = input_token.Count();
		}

		gss_buffer_desc output_buffer = GSS_C_EMPTY_BUFFER;
		OM_uint32 minor = 0;
		const OM_uint32 major = gss_init_sec_context(
			&minor,
			GSS_C_NO_CREDENTIAL,
			&data->context,
			data->target_name,
			GSS_C_NO_OID,
			GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG,
			0,
			GSS_C_NO_CHANNEL_BINDINGS,
			input_token.Count() > 0 ? &input_buffer : GSS_C_NO_BUFFER,
			nullptr,
			&output_buffer,
			nullptr,
			nullptr
		);
		if(GSS_ERROR(major))
		{
			if(output_buffer.length > 0)
				gss_release_buffer(&minor, &output_buffer);
			EL_THROW(TGssapiException, major, minor);
		}

		TInitiatorStep result;
		if(output_buffer.length > 0)
			result.output_token.Append((const byte_t*)output_buffer.value, output_buffer.length);
		gss_release_buffer(&minor, &output_buffer);
		result.complete = major == GSS_S_COMPLETE;
		return result;
	}
}
