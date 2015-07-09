#pragma once
#include <functional>
#include <type_traits>
#include "event_loop.h"
#include "promise.h"

namespace mark {

using namespace std;

class AbstractEventLoop;

template <typename ResultPromise, typename... Args>
class Task {
public:
	using Target = function<ResultPromise(AbstractEventLoop& loop, Args...)>;
	Task(const EventLoopPool pool, Target& func);
	template <typename = typename enable_if<!is_void<ResultPromise>::value>::type>
		void operator ()(AbstractEventLoop& loop, Args&&... args);
	template <typename = typename enable_if<is_void<ResultPromise>::value>::type>
		ResultPromise operator ()(AbstractEventLoop& loop, Args&&... args);
private:
	EventLoopPool pool;
	Target func;
};

}

#include "task.tpp"
