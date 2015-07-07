#pragma once
#include <mutex>
#include <condition_variable>
#include <atomic>

namespace mark {

/*
 * Trigger
 *
 * An object that threads can wait on.  Ordering is respected: waiting threads
 * are resumed in a FIFO manner when triggered individually by trigger_one.  The
 * last n threads to wait will be released by trigger_n.
 *
 *  * trigger_one will cause the next wait_for to return immediately if no
 *    threads are waiting on the trigger.
 *
 *  * trigger_n behaves like trigger_one called n times (although the
 *    implementation is O(1) actually).
 *
 *  * trigger_all triggers all currently waiting threads.  future threads that
 *    call wait_for will not be triggered immediately by extra
 *    trigger_one/trigger_n calls if trigger_all is called between the extra
 *    trigger_one/trigger_n and wait_for.
 *
 *  * disable causes all waiting threads to resume and causes future waits to
 *    return immediately.
 *
 *  * wait_for returns true unless the wait was terminated by a call to
 *    disable() and definitely not by any pending trigger_one/trigger_n calls.
 *
 * I recommend deciding when you design, to either use only the
 * trigger_one/trigger_n, or only the trigger_all methods, but not both sets.
 *
 * Example of future ordering:
 *
 *  1. START
 *  2. trigger_one called
 *  3. thread calls wait_for
 *  4. thread resumes immediately
 *  4. END
 *
 *  1. START
 *  2. thread calls wait_for
 *  3. trigger_n(2) called (or trigger_one twice)
 *  4. thread resumes
 *  5. thread calls wait_for
 *  6. thread resumes immediately
 *  7. END
 *
 *  1. START
 *  2. thread calls wait_for
 *  3. trigger_n(2) called (or trigger_one twice)
 *  4. thread resumes
 *  5. trigger_all is called
 *  6. thread calls wait_for
 *  7. thread waits until a trigger_* method is called (trigger_all cleared the
 *     pending resume left over from trigger_n(2))
 *  8. END
 */
class Trigger {
public:
	Trigger(const Trigger&) = delete;
	Trigger& operator =(const Trigger&) = delete;
	Trigger() { wait_start = 0; wait_end = 0; wait_count = 0; disabled = false; };
	/* Blocks and waits for trigger */
	bool wait_for();
	/* Triggers one waiting thread */
	void trigger_one();
	/* Triggers n waiting threads */
	void trigger_n(const int n);
	/* Triggers all currently waiting threads */
	void trigger_all();
	/* Causes all waits to return and future waits to return immediately */
	void disable();
	bool is_disabled() const { return disabled; };
	/* Number of waiting threads */
	int get_wait_count() const { return wait_count; };
private:
	std::mutex mutex;
	std::condition_variable cv;
	std::atomic<bool> disabled;
	std::atomic<int> wait_start;
	std::atomic<int> wait_end;
	std::atomic<int> wait_count;
};

}
