#include "spinlock.h"

namespace mark {

using namespace std;

UserspaceSpinlock::UserspaceSpinlock(atomic_flag& flag) : flag(flag)
{
	while (flag.test_and_set(memory_order_acquire))
		;
}

UserspaceSpinlock::~UserspaceSpinlock()
{
	flag.clear(memory_order_release);
}

}
