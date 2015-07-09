#pragma once
#include <atomic>

namespace mark {

using namespace std;

/*
 * Lightweight userspace spinlock
 *
 * Is not re-entrant - attempting to acquire the lock twice in the same thread
 * will deadlock.
 */
class UserspaceSpinlock {
public:
	UserspaceSpinlock(const UserspaceSpinlock&) = delete;
	UserspaceSpinlock(const UserspaceSpinlock&&) = delete;
	UserspaceSpinlock operator =(const UserspaceSpinlock&) = delete;
	UserspaceSpinlock(atomic_flag& flag);
	~UserspaceSpinlock();
private:
	atomic_flag& flag;
};

}
