#include "io_net_url.hpp"
#include "io_text_parser.hpp"

namespace el1::io::net::url
{
	using namespace error;
	using namespace text::parser;

	namespace
	{
		constexpr char32_t UNICODE_MAX = (char32_t)0x10ffff;

		static std::optional<u16_t> ParsePort(const TStringView text)
		{
			u32_t value = 0;
			for(const char32_t chr : text)
			{
				const u32_t digit = (u32_t)(chr - U'0');
				if(value > (65535U - digit) / 10U)
					return std::nullopt;
				value = value * 10U + digit;
			}
			return value > 0 ? std::optional<u16_t>((u16_t)value) : std::nullopt;
		}

		static auto MakeParser()
		{
			auto any = CharRange((char32_t)0, UNICODE_MAX);
			auto url_char = ~CharList(U'?', U'#');
			auto scheme_first = CharRange(U'A', U'Z') || CharRange(U'a', U'z');
			auto scheme_next = scheme_first || CharRange(U'0', U'9') || CharList(U'+', U'-', U'.');
			auto scheme = Capture(scheme_first + Repeat(0, NEG1, scheme_next)) + Discard(U':'_P);

			auto user_info = Maybe(Capture(Repeat(0, NEG1, ~CharList(U'@', U'/', U'?', U'#'))) + Discard(U'@'_P));
			auto bracketed_host = Between(U'['_P, Capture(OneOrMore(~CharList(U']', U'/', U'?', U'#'))), U']'_P);
			auto plain_host = Capture(OneOrMore(~CharList(U':', U'@', U'/', U'?', U'#')));
			auto host = bracketed_host || plain_host;
			auto port = Maybe(Discard(U':'_P) + TryTranslate(ParsePort, Capture(OneOrMore(CharRange(U'0', U'9')))));

			auto authority = Translate(
				[](std::optional<TStringView> user_info, const TStringView host, std::optional<u16_t> port)
				{
					TUrl result;
					result.user_info = user_info ? TString(*user_info) : TString();
					result.host = TString(host);
					result.port = port.value_or(0);
					result.port_explicit = port.has_value();
					result.has_authority = true;
					return result;
				},
				user_info, host, port
			);

			auto path_abempty = Capture(Maybe(U'/'_P + Repeat(0, NEG1, url_char)));
			auto authority_path = Translate(
				[](TUrl result, const TStringView path)
				{
					result.path = path.Length() > 0 ? TString(path) : TString(U"/");
					return result;
				},
				Discard(U"//"_P) + Expect(authority), path_abempty
			);

			auto path_any = Translate(
				[](const TStringView path)
				{
					TUrl result;
					result.path = TString(path);
					return result;
				},
				Capture(Repeat(0, NEG1, url_char))
			);

			auto absolute = Translate(
				[](const TStringView scheme, TUrl result)
				{
					result.scheme = TString(scheme);
					result.scheme.ToLower();
					return result;
				},
				scheme, authority_path || path_any
			);

			auto path_absolute = Capture(U'/'_P + Repeat(0, NEG1, url_char));
			auto path_noscheme = Capture(OneOrMore(~CharList(U':', U'/', U'?', U'#')) + Maybe(U'/'_P + Repeat(0, NEG1, url_char)));
			auto path_empty = Capture(Repeat(0, 0, any));
			auto relative_path = Translate(
				[](const TStringView path)
				{
					TUrl result;
					result.path = TString(path);
					return result;
				},
				path_absolute || path_noscheme || path_empty
			);
			auto relative = authority_path || relative_path;

			auto query = Maybe(Discard(U'?'_P) + Capture(Repeat(0, NEG1, ~CharList(U'#'))));
			auto fragment = Maybe(Discard(U'#'_P) + Capture(Repeat(0, NEG1, any)));

			return Translate(
				[](TUrl result, std::optional<TStringView> query, std::optional<TStringView> fragment)
				{
					result.query = query ? TString(*query) : TString();
					result.fragment = fragment ? TString(*fragment) : TString();
					if(!result.port_explicit)
						result.port = TUrl::DefaultPort(result.scheme);
					return result;
				},
				absolute || relative, query, fragment
			);
		}
	}

	u16_t TUrl::DefaultPort(const TString& scheme)
	{
		if(scheme == U"http")
			return 80;
		if(scheme == U"https")
			return 443;
		return 0;
	}

	TUrl TUrl::FromString(const TStringView text)
	{
		return EL_ANNOTATE_ERROR(MakeParser().Parse(text), TInvalidArgumentException, "text", "invalid URL");
	}

	TString TUrl::RequestTarget() const
	{
		TString result = path.Length() == 0 ? TString(U"/") : path;
		if(query.Length() > 0)
			result += TString(U"?") + query;
		return result;
	}
}
