#pragma once

#include "error.hpp"
#include "util.hpp"
#include "io_types.hpp"
#include "io_stream.hpp"
#include "io_collection_list.hpp"
#include "io_collection_map.hpp"
#include "io_text_encoding.hpp"

namespace el1::io::text
{
	using namespace io::types;

	namespace string
	{
		class TString;
	}

	enum class EPlacement : u8_t
	{
		NONE,
		START,
		MID,
		END,
	};

	template<typename T>
	class TCStrPipe : public stream::IPipe<TCStrPipe<T>, T>
	{
		protected:
			const T* cstr;
			const T* const end;

		public:
			using TOut = T;
			using TIn = void;

			const TOut* NextItem() final override
			{
				if(cstr < end && *cstr != '\0')
					return cstr++;
				else
					return nullptr;
			}

			TCStrPipe(const T* const cstr, const usys_t n_max_chars = NEG1) : cstr(cstr), end(n_max_chars == NEG1 ? (const T*)NEG1 : cstr + n_max_chars) {}

			TCStrPipe(const io::collection::list::array_t<const T> array) : cstr(array.ItemPtr(0)), end(array.ItemPtr(0) + array.Count()) {}
	};
}
