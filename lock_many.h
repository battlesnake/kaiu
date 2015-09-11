#pragma once
#include <mutex>
#include <vector>

namespace kaiu {


/* Lock multiple mutexes while avoiding deadlocks */
class lock_many {
public:
	template <typename It>
	lock_many(It begin, It end);
	lock_many() = delete;
private:
	std::vector<std::unique_lock<std::mutex>> locks;
};

}

#ifndef lock_many_tcc
#include "lock_many.tcc"
#endif
