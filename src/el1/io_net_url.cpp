#include "io_net_url.hpp"
#include "io_text_parser.hpp"

namespace el1::io::net::url
{
	using namespace error;
	using namespace text::parser;

	namespace
	{
		constexpr char32_t UNICODE_MAX = (char32_t)0x10ffff;

		struct syntax_t
		{
			TString scheme;
			TString authority;
			TString path;
			TString query;
			TString fragment;
			bool has_authority = false;
		};

		struct authority_t
		{
			TString user_info;
			TString host;
			TString port;
			bool port_explicit = false;
		};

		static TString OptionalText(const TList<TStringView>& values)
		{
			return values.Count() == 0 ? TString() : TString(values[0]);
		}

		static syntax_t ParseSyntax(const TString& url)
		{
			auto scheme_first = CharRange(U'A', U'Z') || CharRange(U'a', U'z');
			auto scheme_next = scheme_first || CharRange(U'0', U'9') || CharList(U'+', U'-', U'.');
			auto scheme = Optional(Capture(scheme_first + Repeat(scheme_next, 0, NEG1)) + Discard(U':'_P));
			auto authority = Optional(Discard(U"//"_P) + Capture(Repeat(~CharList(U'/', U'?', U'#'), 0, NEG1)));
			auto path = Capture(Repeat(~CharList(U'?', U'#'), 0, NEG1));
			auto query = Optional(Discard(U'?'_P) + Capture(Repeat(~CharList(U'#'), 0, NEG1)));
			auto fragment = Optional(Discard(U'#'_P) + Capture(Repeat(CharRange((char32_t)0, UNICODE_MAX), 0, NEG1)));

			auto parser = Translate(
				[](TList<TStringView> scheme_value, TList<TStringView> authority_value, TStringView path_value, TList<TStringView> query_value, TList<TStringView> fragment_value)
				{
					syntax_t result;
					result.scheme = OptionalText(scheme_value);
					result.authority = OptionalText(authority_value);
					result.path = TString(path_value);
					result.query = OptionalText(query_value);
					result.fragment = OptionalText(fragment_value);
					result.has_authority = authority_value.Count() != 0;
					return result;
				},
				scheme, authority, path, query, fragment
			);

			return parser.Parse(url);
		}

		static authority_t ParseAuthority(const TString& authority)
		{
			auto user_info = Optional(Capture(Repeat(~CharList(U'@'), 0, NEG1)) + Discard(U'@'_P));
			auto bracketed_host = Between(U'['_P, Capture(OneOrMore(~CharList(U']'))), U']'_P);
			auto plain_host = Capture(OneOrMore(~CharList(U':')));
			auto host = Dispatch(
				Case(U'['_P, bracketed_host),
				Case(~CharList(U'['), plain_host)
			);
			auto port = Optional(Discard(U':'_P) + Capture(OneOrMore(CharRange(U'0', U'9'))));

			auto parser = Translate(
				[](TList<TStringView> user_info_value, TStringView host_value, TList<TStringView> port_value)
				{
					authority_t result;
					result.user_info = OptionalText(user_info_value);
					result.host = TString(host_value);
					result.port = OptionalText(port_value);
					result.port_explicit = port_value.Count() != 0;
					return result;
				},
				user_info, host, port
			);

			return parser.Parse(authority);
		}
	}

	static u16_t ParsePort(const TString& value)
	{
		EL_ERROR(value.Length() == 0, TInvalidArgumentException, "url", "empty port");
		const u64_t port = value.ToInteger();
		EL_ERROR(port == 0 || port > 65535U, TInvalidArgumentException, "url", "port must be between 1 and 65535");
		return (u16_t)port;
	}

	u16_t TUrl::DefaultPort(const TString& scheme)
	{
		if(scheme == U"http")
			return 80;
		if(scheme == U"https")
			return 443;
		return 0;
	}

	TUrl::parsed_t TUrl::Parse(const TString& url)
	{
		parsed_t result;
		syntax_t syntax;
		try
		{
			syntax = ParseSyntax(url);
		}
		catch(const TException&)
		{
			EL_THROW(TInvalidArgumentException, "url", "invalid URL syntax");
		}

		if(syntax.scheme.Length() == 0)
		{
			const usys_t colon = syntax.path.Find(U':');
			const usys_t slash = syntax.path.Find(U'/');
			EL_ERROR(colon != NEG1 && (slash == NEG1 || colon < slash), TInvalidArgumentException, "url", "invalid URL scheme");
		}
		else
		{
			result.scheme = syntax.scheme;
			result.scheme.ToLower();
		}

		result.has_authority = syntax.has_authority;
		if(result.has_authority)
		{
			EL_ERROR(syntax.authority.Length() == 0, TInvalidArgumentException, "url", "missing authority");
			authority_t authority;
			try
			{
				authority = ParseAuthority(syntax.authority);
			}
			catch(const TException&)
			{
				EL_THROW(TInvalidArgumentException, "url", "invalid URL authority");
			}

			result.user_info = authority.user_info;
			result.host = authority.host;
			result.port_explicit = authority.port_explicit;
			if(authority.port_explicit)
				result.port = ParsePort(authority.port);
		}

		result.path = syntax.path;
		result.query = syntax.query;
		result.fragment = syntax.fragment;
		if(result.has_authority && result.path.Length() == 0)
			result.path = TString(U"/");
		if(result.port == 0)
			result.port = DefaultPort(result.scheme);

		return result;
	}

	TUrl::TUrl(const parsed_t& parsed) :
		scheme(parsed.scheme),
		user_info(parsed.user_info),
		host(parsed.host),
		port(parsed.port),
		port_explicit(parsed.port_explicit),
		has_authority(parsed.has_authority),
		path(parsed.path),
		query(parsed.query),
		fragment(parsed.fragment)
	{
	}

	TUrl::TUrl(const TString& url) : TUrl(Parse(url))
	{
	}

	TString TUrl::RequestTarget() const
	{
		TString result = path.Length() == 0 ? TString(U"/") : path;
		if(query.Length() > 0)
			result += TString(U"?") + query;
		return result;
	}
}
