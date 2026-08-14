#include "security_jwt.hpp"

#include "io_collection_list.hpp"
#include "io_format_base64.hpp"
#include "io_text_encoding_utf8.hpp"
#include "system_time.hpp"

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

namespace el1::security::jwt
{
	using namespace el1::error;
	using namespace el1::io::collection::list;
	using namespace el1::io::format::base64;
	using namespace el1::io::text::encoding::utf8;
	using namespace el1::system::time;

	struct TJwkSet::data_t
	{
		struct key_t
		{
			TString kid;
			TString algorithm;
			EVP_PKEY* key = nullptr;
		};

		TList<key_t> keys;

		EVP_PKEY* FindKey(const TString& kid, const TString& algorithm) const
		{
			for(const auto& key : keys)
				if(key.kid == kid && (key.algorithm.Length() == 0 || key.algorithm == algorithm))
					return key.key;
			return nullptr;
		}

		~data_t()
		{
			for(auto& key : keys)
				EVP_PKEY_free(key.key);
		}
	};

	static TList<byte_t> ToBytes(const TString& text)
	{
		return text.chars.Pipe().Transform(TUTF8Encoder()).Collect();
	}

	static TString FromBytes(const TList<byte_t>& bytes)
	{
		return TString(bytes.Pipe().Transform(TUTF8Decoder()).Collect());
	}


	static EVP_PKEY* RsaJwkToKey(const TJsonValue& jwk)
	{
		const TList<byte_t> modulus = DecodeBase64Url(jwk("n").String());
		const TList<byte_t> exponent = DecodeBase64Url(jwk("e").String());
		EL_ERROR(modulus.Count() == 0 || exponent.Count() == 0, TInvalidArgumentException, "jwks", "invalid RSA JWK");

		BIGNUM* n = BN_bin2bn(modulus.ItemPtr(0), (int)modulus.Count(), nullptr);
		BIGNUM* e = BN_bin2bn(exponent.ItemPtr(0), (int)exponent.Count(), nullptr);
		RSA* rsa = RSA_new();
		EVP_PKEY* key = EVP_PKEY_new();
		if(n == nullptr || e == nullptr || rsa == nullptr || key == nullptr)
		{
			BN_free(n);
			BN_free(e);
			RSA_free(rsa);
			EVP_PKEY_free(key);
			EL_THROW(TException, "failed to construct RSA key from JWK");
		}
		if(RSA_set0_key(rsa, n, e, nullptr) != 1)
		{
			BN_free(n);
			BN_free(e);
			RSA_free(rsa);
			EVP_PKEY_free(key);
			EL_THROW(TException, "failed to construct RSA key from JWK");
		}
		if(EVP_PKEY_assign_RSA(key, rsa) != 1)
		{
			RSA_free(rsa);
			EVP_PKEY_free(key);
			EL_THROW(TException, "failed to construct RSA key from JWK");
		}
		return key;
	}

	static const EVP_MD* AlgorithmDigest(const TString& algorithm)
	{
		if(algorithm == U"RS256")
			return EVP_sha256();
		if(algorithm == U"RS384")
			return EVP_sha384();
		if(algorithm == U"RS512")
			return EVP_sha512();
		return nullptr;
	}

	static bool AudienceMatches(const TJsonValue& audience, const TString& expected)
	{
		if(expected.Length() == 0)
			return true;
		if(audience.IsString())
			return audience.String() == expected;
		if(audience.IsArray())
			for(const auto& item : audience.Array())
				if(item.IsString() && item.String() == expected)
					return true;
		return false;
	}

	static s64_t NumericClaim(const TJsonValue& value, const s64_t fallback)
	{
		if(value.IsInteger())
			return value.Integer();
		if(value.IsNumber())
			return (s64_t)value.Number();
		return fallback;
	}

	TJwkSet::TJwkSet()
	{
		data = new data_t();
	}

	TJwkSet::TJwkSet(const TJsonValue& jwks) : TJwkSet()
	{
		try
		{
			Load(jwks);
		}
		catch(...)
		{
			delete data;
			data = nullptr;
			throw;
		}
	}

	TJwkSet::~TJwkSet()
	{
		delete data;
	}

	void TJwkSet::Load(const TJsonValue& jwks)
	{
		EL_ERROR(!jwks("keys").IsArray(), TInvalidArgumentException, "jwks", "JWK set does not contain a keys array");

		TList<data_t::key_t> keys;
		try
		{
			for(const auto& jwk : jwks("keys").Array())
			{
				if(jwk("kty").String("") != "RSA")
					continue;
				const TString use = jwk("use").String("");
				if(use.Length() > 0 && use != "sig")
					continue;
				const TString algorithm = jwk("alg").String("");
				if(algorithm.Length() > 0 && AlgorithmDigest(algorithm) == nullptr)
					continue;
				const TString kid = jwk("kid").String("");
				if(kid.Length() == 0)
					continue;

				data_t::key_t key;
				key.kid = kid;
				key.algorithm = algorithm;
				key.key = RsaJwkToKey(jwk);
				keys.Append(key);
			}
			EL_ERROR(keys.Count() == 0, TInvalidArgumentException, "jwks", "JWK set contains no usable RSA signing keys");
		}
		catch(...)
		{
			for(auto& key : keys)
				EVP_PKEY_free(key.key);
			throw;
		}

		for(auto& key : data->keys)
			EVP_PKEY_free(key.key);
		data->keys = std::move(keys);
	}

	TValidationResult TJwkSet::Validate(const TString& token, const TValidationPolicy& policy) const
	{
		TValidationResult result;
		try
		{
			const TList<TString> parts = token.Split(U'.');
			if(parts.Count() != 3)
				return result;

			result.header = TJsonValue::Parse(FromBytes(DecodeBase64Url(parts[0])));
			const TString algorithm = result.header("alg").String("");
			const EVP_MD* const digest = AlgorithmDigest(algorithm);
			if(digest == nullptr)
			{
				result.error = EValidationError::UNSUPPORTED_ALGORITHM;
				return result;
			}
			const TString kid = result.header("kid").String("");
			if(kid.Length() == 0)
				return result;

			EVP_PKEY* const key = data->FindKey(kid, algorithm);
			if(key == nullptr)
			{
				result.error = EValidationError::KEY_NOT_FOUND;
				return result;
			}

			const TList<byte_t> signing_input = ToBytes(parts[0] + TString(U".") + parts[1]);
			const TList<byte_t> signature = DecodeBase64Url(parts[2]);
			EVP_MD_CTX* const context = EVP_MD_CTX_new();
			if(context == nullptr)
				return result;
			const bool signature_valid = EVP_DigestVerifyInit(context, nullptr, digest, nullptr, key) == 1
				&& EVP_DigestVerifyUpdate(context, signing_input.ItemPtr(0), signing_input.Count()) == 1
				&& EVP_DigestVerifyFinal(context, signature.ItemPtr(0), signature.Count()) == 1;
			EVP_MD_CTX_free(context);
			if(!signature_valid)
			{
				result.error = EValidationError::INVALID_SIGNATURE;
				return result;
			}

			result.claims = TJsonValue::Parse(FromBytes(DecodeBase64Url(parts[1])));
			if(policy.issuer.Length() > 0 && result.claims("iss").String("") != policy.issuer)
			{
				result.error = EValidationError::ISSUER_MISMATCH;
				return result;
			}
			if(!AudienceMatches(result.claims("aud"), policy.audience))
			{
				result.error = EValidationError::AUDIENCE_MISMATCH;
				return result;
			}

			const s64_t now = TTime::Now().Seconds();
			const s64_t exp = NumericClaim(result.claims("exp"), 0);
			if(policy.require_expiration && exp == 0)
			{
				result.error = EValidationError::MISSING_EXPIRATION;
				return result;
			}
			if(exp != 0 && exp + policy.clock_skew_seconds <= now)
			{
				result.error = EValidationError::EXPIRED;
				return result;
			}
			const s64_t nbf = NumericClaim(result.claims("nbf"), 0);
			if(nbf != 0 && nbf - policy.clock_skew_seconds > now)
			{
				result.error = EValidationError::NOT_YET_VALID;
				return result;
			}

			result.valid = true;
			result.error = EValidationError::NONE;
			result.expires_unix = exp;
			return result;
		}
		catch(...)
		{
			result.valid = false;
			result.error = EValidationError::MALFORMED;
			return result;
		}
	}
}
