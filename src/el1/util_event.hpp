#pragma once

#include "def.hpp"
#include "io_types.hpp"
#include "io_collection_list.hpp"
#include "util_function.hpp"

namespace el1::util::event
{
	using namespace io::collection::list;
	using namespace util::function;

	template<typename ...A>
	class TEvent
	{
		protected:
			using function_t = TFunction<void, A...>;
			mutable TList<function_t> receivers;

		public:
			const TEvent& operator+=(function_t receiver) const { receivers.Append(receiver); return *this; }
			const TEvent& operator-=(function_t receiver) const { EL_ERROR(receivers.RemoveItem(receiver, 1) != 1, TLogicException); return *this; }

			void Raise(A... a) { receivers.Apply( [a...](function_t& receiver) { receiver(a...); } ); }
	};
}
