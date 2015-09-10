#define scoped_counter_tcc
#include "scoped_counter.h"

namespace kaiu {

using namespace std;

/*** ScopedCounter ***/

template <typename Counter>
ScopedCounter<Counter>::ScopedCounter(const Counter initial_value) :
	value(initial_value)
{
}

template <typename Counter>
void ScopedCounter<Counter>::adjust(const Delta delta)
{
	if (delta == 0) {
		return;
	}
	lock_guard<mutex> lock(zero_cv_mutex);
	value += delta;
	if (value == 0) {
		notify();
	}
}

template <typename Counter>
void ScopedCounter<Counter>::notify() const
{
	zero_cv.notify_all();
}

template <typename Counter>
bool ScopedCounter<Counter>::isZero() const
{
	lock_guard<mutex> lock(zero_cv_mutex);
	return value == 0;
}

template <typename Counter>
void ScopedCounter<Counter>::waitForZero() const
{
	unique_lock<mutex> lock(zero_cv_mutex);
	zero_cv.wait(lock, [this] { return value == 0; });
}

template <typename Counter>
typename ScopedCounter<Counter>::ScopedAdjustment ScopedCounter<Counter>::delta(const Delta amount)
{
	return ScopedAdjustment(*this, amount);
}

/*** ScopedCounter<Counter>::ScopedAdjustment ***/

template <typename Counter>
ScopedCounter<Counter>::ScopedAdjustment::ScopedAdjustment(
	ScopedCounter<Counter>& counter, const Delta delta) :
	counter(counter), delta(delta)
{
	counter.adjust(delta);
}

template <typename Counter>
ScopedCounter<Counter>::ScopedAdjustment::~ScopedAdjustment()
{
	counter.adjust(-delta);
}

template <typename Counter>
ScopedCounter<Counter>::ScopedAdjustment::ScopedAdjustment(ScopedAdjustment&& old) :
	counter(old.counter), delta(0)
{
	/*
	 * Don't call main constructor so we don't alter the counter and
	 * notify_all
	 *
	 * Zero the old instance's delta so it doesn't alter the counter and
	 * notify_all
	 */
	swap(delta, old.delta);
}

}
