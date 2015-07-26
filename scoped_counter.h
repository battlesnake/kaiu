#pragma once
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <type_traits>

namespace kaiu {

using namespace std;

/*
 * Encapsulates an integral value.  Provides a method to temporarily
 * increment/decrement the value by an arbitrary amount, with this change being
 * reversible at a later point.  The change is represented by a ScopedAdjustment object,
 * which can be used with RAII to provide a scope-locked increment/decrement.
 *
 * The condition variable is notified whenever the value has been changed.  A
 * delta of zero produces no change, so will not trigger the condition variable.
 */
template <typename Counter>
class ScopedCounter {
	static_assert(is_integral<Counter>::value,
		"ScopedCounter value type must be integral");
public:
	using Delta = typename make_signed<Counter>::type;
	ScopedCounter(const ScopedCounter&) = delete;
	ScopedCounter(ScopedCounter&&) = delete;
	ScopedCounter(const Counter initial_value = 0);
	~ScopedCounter();
	class ScopedAdjustment {
	public:
		ScopedAdjustment() = delete;
		ScopedAdjustment(const ScopedAdjustment&) = delete;
		ScopedAdjustment(ScopedAdjustment&&);
		ScopedAdjustment(ScopedCounter<Counter>& counter, const Delta delta);
		~ScopedAdjustment();
	private:
		ScopedCounter<Counter>& counter;
		Delta delta;
	};
	ScopedAdjustment delta(const Delta amount);
	bool isZero();
	void waitForZero();
	void notify();
	using Guard = ScopedCounter<Counter>::ScopedAdjustment;
private:
	mutex zero_cv_mutex;
	condition_variable zero_cv;
	atomic<Counter> value{0};
	void adjust(const Delta delta);
};

}

#ifndef scoped_counter_tcc
#include "scoped_counter.tcc"
#endif
