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
#include <chrono>
#include "assertion.h"
#include "decimal.h"
#include "promise.h"

using namespace std;
using namespace std::chrono;
using namespace mark;

const auto cores = thread::hardware_concurrency() > 5 ? thread::hardware_concurrency() : 5;

ParallelEventLoop loop{ {
	{ EventLoopPool::reactor, 1 },
	{ EventLoopPool::interaction, 1 },
	{ EventLoopPool::calculation, cores }
} };

string writeStrFunc(const string& message)
{
	cout << message << endl;
	return message;
}

decimal writeNumFunc(const decimal& value)
{
	cout << "\t" << string(value) << endl;
	return value;
}

decimal factorial(const decimal& x)
{
	return !x;
}

decimal partial_factorial(const decimal& x, const decimal& offset, const decimal& step)
{
	decimal r(1);
	for (decimal i = offset; i <= x; i += step) {
		r *= i;
	}
	return r;
}

decimal partial_factorial_tuple(const tuple<decimal, decimal, decimal>& range)
{
	return partial_factorial(get<0>(range), get<1>(range), get<2>(range));
}

auto writeStr = task::make_factory(loop, writeStrFunc,
	EventLoopPool::interaction, EventLoopPool::reactor);

auto writeNum = task::make_factory(loop, writeNumFunc,
	EventLoopPool::interaction, EventLoopPool::reactor);

auto calcFactorial = task::make_factory(loop, factorial,
	EventLoopPool::calculation, EventLoopPool::reactor);

auto calcPartialFactorial = task::make_factory(loop, partial_factorial_tuple,
	EventLoopPool::calculation, EventLoopPool::reactor);

Assertions assert({
	{ nullptr, "Calculate multiple factorials simultaneously" },
	{ "625", "625!" },
	{ "1250", "1250!" },
	{ "2500", "2500!" },
	{ "5000", "5000!" },
	{ "10000", "10000!" },
	{ nullptr, "Calculate a single factorial in parallel" },
	{ "10001", "10001!" },
});

const auto formatResult = [] (const time_point<system_clock>& start, const int& i) {
	const function<string(const decimal&)> format_result =
		[&start, i] (const decimal& result) {
			const auto duration =
				duration_cast<microseconds>(system_clock::now() - start);
			const string note =
				to_string(result.length()) + " digits \t" +
				"+" + to_string(duration.count() / 1000) + "ms \t" +
				to_string(duration.count() / result.length()) + "μs/digit";
			assert.pass(to_string(i), note);
			return to_string(i) + "! = " + note;
		};
	return task::make_factory<string, const decimal&>(loop, format_result, EventLoopPool::reactor);
};

void calculateMultipleFactorials()
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
	const auto start = system_clock::now();
	/* Calculate multiple factorials */
	for (int i = 625; i <= 10000; i *= 2) {
		promise::resolved<int>(i)
			->finally<int>(jobStart)
			->then_p<decimal>(calcFactorial)
			->then_p<string>(formatResult(start, i))
			->then_p<string>(writeStr)
			->finally(jobEnd);
	}
	unique_lock<mutex> lock(mx);
	cv.wait(lock, [&jobs] { return jobs == 0; });
}

void calculateOneFactorial()
{
	/* Calculate one huge factorial */
	const auto tasks = cores > 4 ? 4 : cores;
	cout << "Using " << tasks << " threads" << endl;
	atomic<bool> done{false};
	mutex mx;
	condition_variable cv;
	const int param = 10001;
	vector<Promise<decimal>> partials;
	partials.reserve(tasks);
	const auto start = system_clock::now();
	for (remove_const<decltype(tasks)>::type i = 0; i < tasks; i++) {
		auto subrange = make_tuple(param, i + 1, tasks);
		partials.push_back(
			promise::resolved(subrange)
				->then_p<decimal>(calcPartialFactorial));
	}
	promise::combine(partials)
		->then<decimal>([&start] (const vector<decimal>& results) {
			decimal reductor{1};
			for (const auto& value : results) {
				reductor *= value;
			}
			return reductor;
		})
		->then_p<string>(formatResult(start, param))
		->then_p<string>(writeStr)
		->finally([&cv, &done] {
			done = true;
			cv.notify_one();
		});
	unique_lock<mutex> lock(mx);
	cv.wait(lock, [&done] { return bool(done); });
}

int main(int argc, char *argv[])
{
	calculateMultipleFactorials();
	calculateOneFactorial();
	this_thread::sleep_for(100ms);
	return assert.print();
}
#endif
