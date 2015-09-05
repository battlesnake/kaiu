#pragma once
#include <mutex>
#include <memory>

namespace kaiu {

using namespace std;

/*
 * We need a way to release all locks when (potentially) destroying the object,
 * so that a RAII lock does not attempt to release the mutex after the object
 * containing the mutex has already been destroyed (destroyed via releasing the
 * self-reference).
 */
class self_managing : public enable_shared_from_this<self_managing> {
protected:
	class ensure_locked_helper {
		friend class self_managing;
		ensure_locked_helper() = delete;
		ensure_locked_helper(mutex& mx) : lock(mx) { }
		void unlock() { lock.unlock(); lock.release(); };
		unique_lock<mutex> lock;
	};
	using ensure_locked = ensure_locked_helper&;
	ensure_locked_helper get_lock() const { return ensure_locked_helper(mx); }
	void make_immortal(ensure_locked) { self_reference = shared_from_this(); }
	void make_mortal(ensure_locked lock) { lock.unlock(); self_reference = nullptr; }
private:
	mutable mutex mx;
	shared_ptr<self_managing> self_reference;
};

}
