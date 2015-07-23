#include "task.h"
#include <iostream>
#include <math.h>

using namespace mark;
using namespace promise;

int sqr(int x)
{
	return x * x;
}

int hypot__(int x, int y)
{
	return sqrt(sqr(x) + sqr(y));
}

void *logOutput(const std::string line, const int value)
{
	std::cout << line << ": " << value << std::endl;
	return nullptr;
}

ParallelEventLoop loop{ {
	{ EventLoopPool::reactor, 1 },
	{ EventLoopPool::calculation, 1 }
} };

auto Hypot = task(factory(hypot__), EventLoopPool::calculation,
	EventLoopPool::reactor) << loop;
auto LogOutput = task(factory(logOutput), EventLoopPool::reactor) << loop;

int main(int argc, char **argv)
{
	/*
	 * Play with Pythagorean triples:
	 * 3:4:5
	 * 5:12:13
	 * 8:13:15
	 */
	auto h3 = Hypot.apply(3);

	h3(4)
		->then(Hypot << 12)
		->then(Hypot << 8)
		->then(LogOutput << "Should be 15");

	loop.join();
	return 0;
}
