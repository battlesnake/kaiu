#define self_locking_tcc
#include "self_locking.h"

namespace kaiu {

using namespace std;

template <typename T, typename... Args>
shared_ptr<T> make_self_locking(Args&&... args)
{
	shared_ptr<T> ptr = make_shared<T>(forward<Args>(args)...);
	ptr->self_weak_reference = ptr;
	return ptr;
}

template <typename Self>
void self_locking<Self>::set_locked(const bool locked)
{
	if (locked) {
		if (!self_strong_reference) {
			self_strong_reference = self_weak_reference.lock();
		}
	} else {
		self_strong_reference = nullptr;
	}
}

}
