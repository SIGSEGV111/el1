#include <gtest/gtest.h>
#include <el1/security_jwt.hpp>

using namespace ::testing;

namespace
{
	using namespace el1::io::format::json;
	using namespace el1::security::jwt;

	static constexpr char32_t TEST_JWKS[] = UR"JSON({"keys":[{"kty":"RSA","kid":"test-key","alg":"RS256","use":"sig","n":"rZ2ArKEr2pcHiT161RPKrwjHXyMJNo42cSq_yu5IQF71qzMIk8mLoQ77iyNRvaFS53nVfRDYPc_LdE54p-t9FwWXMHTFdTJIRWkzPmKrFzrCJStIPnG-pfsKP_Z0iNOEe4nWofQYe-4BMEWBQvMv5zzZE-FRkF71fv6TVeL04MKaKAS7CID6DwrWx9qL5aX2fWZdv_lfiC9IKbAThJ09SKjX_5LiY1eHQ-5ZGyLwcbvMaZO2lqGx3raCOvEW8Y5KF31i4s-_R3EpumNToPLIbvVhD0X3aOChJJtPWhRiJBcClU7hpFJR1E2uhu32MThliJ4qCcd7qONCgXIuwYaRQQ","e":"AQAB"}]})JSON";
	static constexpr char32_t TEST_TOKEN[] = U"eyJhbGciOiJSUzI1NiIsImtpZCI6InRlc3Qta2V5IiwidHlwIjoiSldUIn0.eyJpc3MiOiJodHRwczovL2tleWNsb2FrLmV4YW1wbGUvcmVhbG1zL2ludGVybmFsIiwiYXVkIjoiY29tcGFjdC1yZWdpc3RyeSIsInByZWZlcnJlZF91c2VybmFtZSI6IndyaXRlciIsImV4cCI6NDEwMjQ0NDgwMCwiaWF0IjoxNzAwMDAwMDAwfQ.DkmNcXzxS-qoJ5i2g7ABW_0Ym2kM3hhlnIkt6-BsULLoE_lCq7A2583fiMfvUeBXxa-jhQeR3ytKq7D08uImbyfrJHAzGgwc3eR7QKtqgO10VCukTcTH2DFrRWrIYSPrkgmRDV-IcBjlHxLZVJ4sMwAcBcgHetFRWahZPz9pePD8gXHGdoJjNgUiJa7QSqxDtLH7WwYmfCeeTqtp7a7chWBiyrtwGlkK_TmyFRJUBeBBCWwpQ889YlS1z-eDkGpMNkil_VdhKRY8SmEaMifJ1mJm-b3oeyxV6OsVh6Mn0WV5S60obBrvhErd28Rp5R7sRIHwydHN7P47W7v80M7lmg";

	TEST(security_jwt, validates_rs256_jwk_token)
	{
		TJwkSet keys(TJsonValue::Parse(TEST_JWKS));
		TValidationPolicy policy;
		policy.issuer = el1::io::text::string::TString(U"https://keycloak.example/realms/internal");
		policy.audience = el1::io::text::string::TString(U"compact-registry");
		const TValidationResult result = keys.Validate(TEST_TOKEN, policy);
		EXPECT_TRUE(result.valid);
		EXPECT_EQ(result.error, EValidationError::NONE);
		EXPECT_EQ(result.claims(U"preferred_username").String(), U"writer");
	}

	TEST(security_jwt, reports_policy_and_key_failures)
	{
		TJwkSet keys(TJsonValue::Parse(TEST_JWKS));
		TValidationPolicy policy;
		policy.issuer = el1::io::text::string::TString(U"https://wrong.example/realms/internal");
		policy.audience = el1::io::text::string::TString(U"compact-registry");
		EXPECT_EQ(keys.Validate(TEST_TOKEN, policy).error, EValidationError::ISSUER_MISMATCH);

		policy.issuer = el1::io::text::string::TString(U"https://keycloak.example/realms/internal");
		policy.audience = el1::io::text::string::TString(U"other-service");
		EXPECT_EQ(keys.Validate(TEST_TOKEN, policy).error, EValidationError::AUDIENCE_MISMATCH);

		EXPECT_EQ(keys.Validate(el1::io::text::string::TString(U"not-a-token"), policy).error, EValidationError::MALFORMED);
	}
}
