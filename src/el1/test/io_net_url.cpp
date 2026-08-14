#include <gtest/gtest.h>
#include <el1/io_net_url.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::io::net::url;

	TEST(io_net_url, TUrl_absolute_http)
	{
		const TUrl url = TUrl::FromString(U"https://user@example.com:8443/a/b?x=1#fragment");
		EXPECT_EQ(url.authority.scheme, U"https");
		EXPECT_EQ(url.authority.username, U"user");
		EXPECT_EQ(url.authority.host, U"example.com");
		EXPECT_EQ(url.authority.port, 8443);
		EXPECT_EQ(url.path, U"/a/b");
		EXPECT_EQ(url.query, U"x=1");
		EXPECT_EQ(url.fragment, U"fragment");
		EXPECT_EQ(url.RequestTarget(), U"/a/b?x=1");
	}

	TEST(io_net_url, TUrl_defaults_ipv6_and_relative)
	{
		const TUrl https = TUrl::FromString(U"HTTPS://[2001:db8::1]/token");
		EXPECT_EQ(https.authority.scheme, U"https");
		EXPECT_EQ(https.authority.host, U"2001:db8::1");
		EXPECT_EQ(https.authority.port, 0);

		const TUrl relative = TUrl::FromString(U"/callback?code=abc&state=xyz");
		EXPECT_EQ(relative.authority.scheme, U"");
		EXPECT_EQ(relative.path, U"/callback");
		EXPECT_EQ(relative.query, U"code=abc&state=xyz");
		EXPECT_EQ(relative.RequestTarget(), U"/callback?code=abc&state=xyz");
	}

	TEST(io_net_url, TUrl_rejects_invalid_syntax)
	{
		EXPECT_THROW(TUrl::FromString(U"https://example.com:70000/"), el1::error::TInvalidArgumentException);
		EXPECT_THROW(TUrl::FromString(U"https://example.com:999999999999999999999999999999999999999/"), el1::error::TInvalidArgumentException);
		EXPECT_THROW(TUrl::FromString(U"https://2001:db8::1/"), el1::error::TInvalidArgumentException);
		EXPECT_THROW(TUrl::FromString(U"https://[2001:db8::1/token"), el1::error::TInvalidArgumentException);
		EXPECT_THROW(TUrl::FromString(U"1https://example.com/"), el1::error::TInvalidArgumentException);
	}

	TEST(io_net_url, TUrl_parser_handles_empty_components_and_network_path)
	{
		const TUrl network = TUrl::FromString(U"//user@example.com:8080?");
		EXPECT_EQ(network.authority.username, U"user");
		EXPECT_EQ(network.authority.host, U"example.com");
		EXPECT_EQ(network.authority.port, 8080);
		EXPECT_EQ(network.path, U"/");
		EXPECT_EQ(network.query, U"");

		const TUrl relative = TUrl::FromString(U"foo/bar:baz#");
		EXPECT_EQ(relative.path, U"foo/bar:baz");
		EXPECT_EQ(relative.fragment, U"");
	}
}
