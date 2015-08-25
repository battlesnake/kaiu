#pragma once
#include <memory>

namespace kaiu {

using namespace std;

template <typename Self>
class self_locking {
protected:
	/*
	 * Prevent destruction via self-reference.  Does not count locks so do not
	 * nest them.  Locking during construction has no effect as the weak
	 * reference is only assigned after construction.
	 *
	 * This gets around the UB which occurs when calling shared_from_this of
	 * enable_shared_from_this descendants if no shared_ptr has yet been set up.
	 */
	void set_locked(const bool locked);
private:
	/*
	 * Self-references, used to control own lifetime and to allow an object to
	 * move between scopes (e.g. via callbacks on non-blocking async operations)
	 */
	weak_ptr<Self> self_weak_reference;
	shared_ptr<Self> self_strong_reference;
	template <typename T, typename... Args>
	friend shared_ptr<T> make_self_locking(Args&&...);
};

template <typename T, typename... Args>
shared_ptr<T> make_self_locking(Args&&...);

}

#ifndef self_locking_tcc
#include "self_locking.tcc"
#endif
