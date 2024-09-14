#include "security_kerberos.hpp"

namespace el1::security::kerberos
{
	using namespace io::text::string;
	using namespace error;

	TString TKerberosException::Message() const
	{
		return message;
	}

	IException* TKerberosException::Clone() const
	{
		return new TKerberosException(*this);
	}

	static TString GetErrorMessage(const TKerberosContext& context, const krb5_error_code code)
	{
		const char *error_message = krb5_get_error_message(context.context, code);
		TString message = error_message;
		krb5_free_error_message(context.context, error_message);
		return message;
	}

	TKerberosException::TKerberosException(const TKerberosContext& context, const krb5_error_code code) : code(code), message(GetErrorMessage(context, code))
	{}
}
