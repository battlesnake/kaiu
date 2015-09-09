#pragma once
#include <mutex>
#include <memory>

namespace kaiu {

/*
 * We need a way to release all locks when (potentially) destroying the object,
 * so that a RAII lock does not attempt to release the mutex after the object
 * containing the mutex has already been destroyed (destroyed via releasing the
 * self-reference).
 */
class self_managing : public std::enable_shared_from_this<self_managing> {
protected:
	/* Helper class, lightweight and easy to move, lives on the stack */
	class self_managing_helper final {
	public:
		self_managing_helper() { }
		/* No copy */
		self_managing_helper(const self_managing_helper&) = delete;
		self_managing_helper& operator =(const self_managing_helper&) = delete;
		/* Move constructor */
		self_managing_helper(self_managing_helper&& from) = default;
		self_managing_helper& operator =(self_managing_helper&& from) = default;
		~self_managing_helper()
		{
			if (lock) {
				lock.unlock();
				lock.release();
			}
			/* May trigger destruction of object */
			if (immortality) {
				immortality->reset();
			}
		}
	private:
		friend class self_managing;
		std::unique_lock<std::mutex> lock;
		/*
		 * An ensure_locked lock helper can be used to return an object to
		 * mortality.  The actual return to mortality occurs at the end of the
		 * helper's scope.
		 *
		 * Using a unique_ptr rather than raw pointer to the shared_ptr as the
		 * pointer should zero itself when move'd out.
		 */
		struct noop_deleter { void operator () (void const * const) const { } };
		std::unique_ptr<std::shared_ptr<self_managing>, noop_deleter> immortality;
		/*
		 * Instructs this helper to reset this shared_ptr when the lock helper
		 * destroyed (lock may be released prior to then by unlock() method.
		 *
		 * Used to defer make_mortal's action to the end of the lock scope.
		 */
		void clear_ptr_at_end_of_lock_scope(std::shared_ptr<self_managing>& ref)
		{
			immortality.reset(&ref);
		}
		/*
		 * Constructor takes reference to the owning object's mutex, and locks
		 * it.  A shared_ptr refrence to the object must already exist.
		 */
		explicit self_managing_helper(std::mutex& mx) :
			lock(mx), immortality(nullptr)
				{ }
	};
	/*
	 * All methods which must be called from within a lock shall take
	 * ensure_locked as their first parameter, to give a compile guarantee that
	 * they are called from within a lock.
	 *
	 * Never call a member method that does not require a lock parameter from
	 * within a lock.
	 */
	using ensure_locked = self_managing_helper&;
	/*
	 * Acquire a lock
	 */
	self_managing_helper get_lock() const
	{
		return self_managing_helper(mx);
	}
	/*
	 * Make immortal allows an object accessed via external shared_ptr wrappers
	 * to out-live the scope of those wrappers, by taking a self reference.
	 *
	 * Useful for asynchronous non-blocking stuff.
	 */
	void make_immortal(ensure_locked)
	{
		self_reference = shared_from_this();
	}
	/*
	 * Instructs the lock helper to clear the self reference shared_ptr of this
	 * object when the lock helper is destroyed.  Note that the lock may be
	 * released long before that point, if the unlock() method is explicitly
	 * called by your code.
	 *
	 * We ensure that the lock will be released before we destroy the object
	 * and the mutex.
	 */
	void make_mortal(ensure_locked lock)
	{
		lock.clear_ptr_at_end_of_lock_scope(self_reference);
	}
private:
	mutable std::mutex mx;
	std::shared_ptr<self_managing> self_reference;
};

}
