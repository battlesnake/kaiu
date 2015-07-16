#pragma once
#include <mutex>
#include <condition_variable>

namespace mark {

using namespace std;

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
	StarterPistol(const int racers);
	void ready();
private:
	int racers{0};
	condition_variable trigger;
	mutex trigger_mutex;
};

}
