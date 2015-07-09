#pragma once
#include <functional>
#include <type_traits>
#include "event_loop.h"
#include "promise.h"

namespace mark {

using namespace std;

class AbstractEventLoop;

template <typename Result, typename... Args>
class Task {
public:
	using ResultPromise = Promise<Result>;
	using Action = function<ResultPromise(AbstractEventLoop& loop, Args...)>;
	Task(const EventLoopPool pool, Action& action);
	Task(const EventLoopPool pool, Action&& action);
	ResultPromise operator ()(AbstractEventLoop& loop, Args&&... args);
private:
	EventLoopPool pool;
	Action action;
};

}

#include "task.tcc"
