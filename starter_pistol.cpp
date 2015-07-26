#include "starter_pistol.h"

namespace kaiu {

StarterPistol::StarterPistol(const int racers) :
	racers(racers)
{
}

void StarterPistol::ready()
{
	unique_lock<mutex> lock(trigger_mutex);
	if (--racers == 0) {
		trigger.notify_all();
	} else {
		trigger.wait(lock, [this] { return racers == 0; });
	}
}

}
