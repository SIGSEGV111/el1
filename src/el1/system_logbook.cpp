#include "system_logbook.hpp"

namespace el1::system::logbook
{
	void test()
	{
		WriteLog(ECategory::LIVENESS, (void*)nullptr, U"hello world %d", 17);
	}
}
