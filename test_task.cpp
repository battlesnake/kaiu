#include <iostream>
#include <sstream>
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
#include "task.h"

using namespace std;
using namespace std::chrono;
using namespace kaiu;

Assertions assert({
	{ nullptr, "Behaviour" },
	{ "THREADS", "Callbacks are dispatched to correct threads" },
	{ nullptr, "Calculate multiple factorials simultaneously" },
	{ "625", "625!" },
	{ "1250", "1250!" },
	{ "2500", "2500!" },
	{ "5000", "5000!" },
	{ "10000", "10000!" },
	{ nullptr, "Calculate a single factorial in parallel" },
	{ "10001", "10001!" },
});

const auto cores = thread::hardware_concurrency() > 0 ? thread::hardware_concurrency() : 4;

ParallelEventLoop loop{ {
	{ EventLoopPool::reactor, 1 },
	{ EventLoopPool::interaction, 1 },
	{ EventLoopPool::calculation, cores }
} };

void threadTests()
{
	atomic<bool> done{false};
	mutex mx;
	condition_variable cv;
	const auto doReaction = [&] (int result) {
		if (result != 42) {
			assert.fail("THREADS", "Result incorrect");
		}
		if (ParallelEventLoop::current_pool() != EventLoopPool::reactor) {
			assert.fail("THREADS", "Reaction in wrong pool");
		}
		assert.pass("THREADS");
	};
	auto handler = [&] (exception_ptr) {
		if (ParallelEventLoop::current_pool() != EventLoopPool::reactor) {
			assert.fail("THREADS", "Reaction (handler) in wrong pool");
		}
		assert.fail("THREADS", "Exception thrown");
	};
	auto finalizer = [&] () {
		if (ParallelEventLoop::current_pool() != EventLoopPool::reactor) {
			assert.fail("THREADS", "Reaction (finalizer) in wrong pool");
		}
		done = true;
		cv.notify_all();
	};
	const auto doCalculation = [&] () {
		if (ParallelEventLoop::current_pool() != EventLoopPool::calculation) {
			assert.fail("THREADS", "Action in wrong pool");
		}
		Promise<int> promise;
		/* Asynchronous */
		loop.push(EventLoopPool::calculation,
			[=] (EventLoop&) {
				this_thread::sleep_for(10ms);
				promise->resolve(42);
			});
		return promise;
	};
	const auto handleInteraction = [&] (EventLoop& loop) {
		if (ParallelEventLoop::current_pool() != EventLoopPool::interaction) {
			assert.fail("THREADS", "Initial job in wrong pool");
		}
		const auto task = promise::task(promise::Factory<int>(doCalculation),
			EventLoopPool::calculation, EventLoopPool::reactor) << ref(loop);
		task()
			->then(doReaction, handler, finalizer);
	};
	loop.push(EventLoopPool::interaction, handleInteraction);
	unique_lock<mutex> lock(mx);
	cv.wait(lock, [&done] { return bool(done); });
}

void behaviourTests()
{
	threadTests();
}

string writeStrFunc(const string& message)
{
	//cout << message << endl;
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
	if (offset > x) {
		return decimal(1);
	}
	decimal r(offset);
	for (decimal i = offset + step; i <= x; i += step) {
		r *= i;
	}
	return r;
}

decimal partial_factorial_tuple(const tuple<decimal, decimal, decimal>& range)
{
	return partial_factorial(get<0>(range), get<1>(range), get<2>(range));
}

string format_result(const time_point<system_clock>& start, const int& i, const decimal& result)
{
	const auto duration =
		duration_cast<microseconds>(system_clock::now() - start);
	const string note =
		to_string(result.length()) + " digits \t" +
		"+" + to_string(duration.count() / 1000) + "ms \t" +
		to_string(duration.count() / result.length()) + "μs/digit";
	assert.pass(to_string(i), note);
	return to_string(i) + "! = " + note;
}

decimal series_product(const vector<decimal>& series)
{
	if (series.size() == 0) {
		throw invalid_argument("Product of empty series");
	}
	decimal reductor{series[0]};
	bool first = true;
	for (const auto& value : series) {
		if (first) {
			first = false;
			continue;
		}
		reductor = decimal::parallel_multiply(reductor, value);
		//reductor *= value;
	}
	return reductor;
}

void print_error(exception_ptr error)
{
	try {
		rethrow_exception(error);
	} catch (const exception& e) {
		cerr << "Exception!  Message: " << e.what() << endl;
	}
}

const auto writeStr = promise::dispatchable(writeStrFunc,
	EventLoopPool::interaction, EventLoopPool::reactor) << ref(loop);

const auto writeNum = promise::dispatchable(writeNumFunc,
	EventLoopPool::interaction, EventLoopPool::reactor) << ref(loop);

const auto calcFactorial = promise::dispatchable(factorial,
	EventLoopPool::calculation, EventLoopPool::reactor) << ref(loop);

const auto calcPartialFactorial = promise::dispatchable(partial_factorial_tuple,
	EventLoopPool::calculation, EventLoopPool::reactor) << ref(loop);

const auto formatResult = promise::dispatchable(format_result,
	EventLoopPool::interaction, EventLoopPool::reactor) << ref(loop);

const auto seriesProduct = promise::dispatchable(series_product,
	EventLoopPool::calculation, EventLoopPool::reactor) << ref(loop);

void calculateMultipleFactorials()
{
	/*
	 * Use decimal class to calculate big factorials then display their
	 * lengths
	 */
	const auto start = system_clock::now();
	/* Calculate multiple factorials */
	for (int i = 625; i <= 10000; i *= 2) {
		promise::resolved(int(i))
			->then(calcFactorial)
			->then(formatResult << cref(start) << i)
			->then(writeStr)
			->finish();
	}
	loop.join(print_error);
}

void calculateOneFactorial()
{
	/* Calculate one huge factorial */
	const auto tasks = cores >= 8 ? 8 : (cores + 1);
	const int x = 10001;
	vector<Promise<decimal&&>> partials;
	partials.reserve(tasks);
	const auto start = system_clock::now();
	for (remove_const<decltype(tasks)>::type i = 0; i < tasks; i++) {
		auto subrange = make_tuple(x, i + 1, tasks);
		partials.push_back(
			promise::resolved(move(subrange))
				->then(calcPartialFactorial));
	}
	promise::combine(partials)
		->then(seriesProduct)
		->then(formatResult << cref(start) << x)
		->then(writeStr)
		->finish();
	loop.join(print_error);
}

int main(int argc, char *argv[])
try {
	behaviourTests();
	calculateMultipleFactorials();
	calculateOneFactorial();
	/* Wait for detatched threads to complete, TODO use OS wait instead */
	this_thread::sleep_for(100ms);
	return assert.print(argc, argv);
} catch (...) {
	assert.print_error();
}
