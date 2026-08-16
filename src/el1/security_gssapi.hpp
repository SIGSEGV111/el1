#pragma once

#include "def.hpp"
#include "error.hpp"
#include "io_types.hpp"
#include "io_collection_list.hpp"
#include "io_text_string.hpp"

namespace el1::security::gssapi
{
	using namespace io::types;
	using namespace io::collection::list;
	using namespace io::text::string;

	struct TGssapiException : error::IException
	{
		const u32_t major_status;
		const u32_t minor_status;
		const TString message;

		TString Message() const final override;
		IException* Clone() const final override;

		TGssapiException(const u32_t major_status, const u32_t minor_status);
	};

	struct TInitiatorStep
	{
		TList<byte_t> output_token;
		bool complete = false;
	};

	class TInitiatorContext
	{
		private:
			struct data_t;
			data_t* data;

		public:
			explicit TInitiatorContext(const TStringView service_name);
			TInitiatorContext(const TInitiatorContext&) = delete;
			TInitiatorContext& operator=(const TInitiatorContext&) = delete;
			~TInitiatorContext();

			TInitiatorStep Process(array_t<const byte_t> input_token = {});
	};
}
