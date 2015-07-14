#include "task.h"

#ifndef test_task
/* REMOVE THIS */
#define test_task
#endif


#ifdef test_task
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include "decimal.h"
#include "promise.h"

using namespace std;
using namespace mark;

ParallelEventLoop loop{ {
	{ EventLoopPool::reactor, 1 },
	{ EventLoopPool::interaction, 1 },
	{ EventLoopPool::calculation, 10 }
} };

void* writeStrFunc(const string& message)
{
	cout << message << endl;
	return nullptr;
}

void* writeNumFunc(const decimal& value)
{
	cout << "\t" << string(value) << endl;
	return nullptr;
}

decimal factorial(const decimal& x)
{
	return !x;
}

auto setExpr(const string& expr, const string& suffix)
{
	return [expr, suffix] (decimal& answer) {
		return promise::resolved(expr + " = " + string(answer) + suffix);
	};
}

auto writeStr = task::make_factory(loop, writeStrFunc,
	EventLoopPool::interaction, EventLoopPool::reactor);

auto writeNum = task::make_factory(loop, writeNumFunc,
	EventLoopPool::interaction, EventLoopPool::reactor);

auto calcFactorial = task::make_factory(loop, factorial,
	EventLoopPool::calculation, EventLoopPool::reactor);

int main(int argc, char *argv[])
{
	/*
	 * Use decimal class to calculate big factorials then display their
	 * lengths
	 *
	 * So tempted to implement dynamic programming and stagger the
	 * start times so bigger operands start earlier...
	 */
	atomic<int> jobs{0};
	mutex mx;
	condition_variable cv;
	auto jobStart = [&jobs] { jobs++; };
	auto jobEnd = [&jobs, &cv] { if (--jobs == 0) { cv.notify_one(); } };
	auto getLength = [] (const decimal& d) {
		return decimal(d.length());
	};
	cout << "STARTED" << endl << endl;
	for (int i = 10000; i <= 20000; i+=1000) {
		promise::resolved<int>(i)
			->finally<int>(jobStart)
			->then_p<decimal>(calcFactorial)
			->then_i<decimal>(getLength)
			->then_p<string>(setExpr(to_string(i) + "!", " digits long"))
			->then_p<void*>(writeStr)
			->finally(jobEnd);
	}
	unique_lock<mutex> lock(mx);
	cv.wait(lock, [&jobs] { return jobs == 0; });
	cout << endl << "ENDED" << endl << endl;
	this_thread::sleep_for(100ms);
	return 0;
}
#endif
