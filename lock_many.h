#pragma once
#include <mutex>
#include <vector>

namespace kaiu {

using namespace std;

/* Lock multiple mutexes while avoiding deadlocks */
class lock_many {
public:
	template <typename It>
	lock_many(It begin, It end);
	lock_many() = delete;
private:
	vector<unique_lock<mutex>> locks;
};

}

#ifndef lock_many_tcc
#include "lock_many.tcc"
#endif
