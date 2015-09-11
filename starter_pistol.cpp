#include <stdexcept>
#include "starter_pistol.h"

namespace kaiu {

using namespace std;

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

void StarterPistol::reset(const int racers_)
{
	if (racers) {
		throw logic_error("Attempted to reset a pending starter pistol");
	}
	racers = racers_;
}

}
