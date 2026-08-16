#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include "io_format_json.hpp"
#include "io_text_string.hpp"

namespace el1::security::jwt
{
	using namespace io::types;
	using namespace io::format::json;
	using namespace io::text::string;

	enum class EValidationError : u8_t
	{
		NONE,
		MALFORMED,
		UNSUPPORTED_ALGORITHM,
		KEY_NOT_FOUND,
		INVALID_SIGNATURE,
		ISSUER_MISMATCH,
		AUDIENCE_MISMATCH,
		MISSING_EXPIRATION,
		EXPIRED,
		NOT_YET_VALID,
	};

	struct TValidationPolicy
	{
		TString issuer;
		TString audience;
		s64_t clock_skew_seconds = 0;
		bool require_expiration = true;
	};

	struct TValidationResult
	{
		bool valid = false;
		EValidationError error = EValidationError::MALFORMED;
		TJsonValue header;
		TJsonValue claims;
		s64_t expires_unix = 0;
	};

	class TJwkSet
	{
		private:
			struct data_t;
			data_t* data = nullptr;

		public:
			TJwkSet();
			explicit TJwkSet(const TJsonValue& jwks);
			TJwkSet(const TJwkSet&) = delete;
			TJwkSet& operator=(const TJwkSet&) = delete;
			~TJwkSet();

			void Load(const TJsonValue& jwks);
			TValidationResult Validate(const TStringView token, const TValidationPolicy& policy = {}) const;
	};
}
