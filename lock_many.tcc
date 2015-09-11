#define lock_many_tcc
#include <mutex>
#include <vector>
#include <algorithm>
#include "lock_many.h"

namespace kaiu {


template <typename It>
lock_many::lock_many(It begin, It end)
{
	/* Map mutexes to unique_lock<mutex> */
	for ( ; begin != end; ++begin) {
		locks.emplace_back(*begin, std::defer_lock);
	}
	const size_t count = locks.size();
	if (count == 0) {
		return;
	}
	size_t start = 0;
	/* Repeatedly try to lock all mutexes */
	do {
		bool failed = false;
		/* Iterate over all mutexes, start from last which failed to lock */
		for (size_t counter = 0; counter < count; counter++) {
			const size_t i = (start + counter) % count;
			auto& lock = locks[i];
			/* First mutex of the loop: Block to lock */
			if (counter == 0) {
				lock.lock();
			/* Subsequent mutexes: Try to lock, fail if we can't */
			} else if (!lock.try_lock()) {
				failed = true;
				/* Next loop will begin by blocking until we can lock this mutex */
				start = i;
				break;
			}
		}
		/* Locked all mutexes? Break out of loop */
		if (!failed) {
			break;
		}
		/* Unlock all if we failed to lock all */
		for (auto& lock : locks) {
			if (lock.owns_lock()) {
				lock.unlock();
			}
		}
	} while (true);
}

}
