#pragma once
#include <mutex>
#include <condition_variable>

namespace kaiu {


/*
 * Starter Pistol
 *
 * Thread synchronization utility
 *
 * Initialize with the number of threads (racers) that you want to synchronize.
 *
 * Each thread calls ready() when it is ready.  This function blocks until the
 * last thread calls it.  All threads then unblock and ready() returns in all of
 * them.
 */
class StarterPistol {
public:
	explicit StarterPistol(const int racers);
	StarterPistol() : StarterPistol(0) { }
	void ready();
	void reset(const int racers);
private:
	int racers{0};
	std::condition_variable trigger;
	std::mutex trigger_mutex;
};

}
