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
#include "task.h"

using namespace std;
using namespace std::chrono;
using namespace mark;

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

const auto cores = thread::hardware_concurrency() > 0 ? thread::hardware_concurrency() : 4;

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
	return move(!x);
}

decimal partial_factorial(const decimal& x, const decimal& offset, const decimal& step)
{
	if (offset > x) {
		return move(decimal(1));
	}
	decimal r(offset);
	for (decimal i = offset + step; i <= x; i += step) {
		r *= i;
	}
	return move(r);
}

decimal partial_factorial_tuple(const tuple<decimal, decimal, decimal>& range)
{
	return move(partial_factorial(get<0>(range), get<1>(range), get<2>(range)));
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
		reductor *= value;
	}
	return move(reductor);
}

auto writeStr = promise::task(promise::factory(writeStrFunc),
	EventLoopPool::interaction, EventLoopPool::reactor) << loop;

auto writeNum = promise::task(promise::factory(writeNumFunc),
	EventLoopPool::interaction, EventLoopPool::reactor) << loop;

auto calcFactorial = promise::task(promise::factory(factorial),
	EventLoopPool::calculation, EventLoopPool::reactor) << loop;

auto calcPartialFactorial = promise::task(promise::factory(partial_factorial_tuple),
	EventLoopPool::calculation, EventLoopPool::reactor) << loop;

const auto formatResult = promise::task(promise::factory(format_result),
	EventLoopPool::interaction, EventLoopPool::reactor) << loop;

auto seriesProduct = promise::task(promise::factory(series_product),
	EventLoopPool::calculation, EventLoopPool::reactor) << loop;

void calculateMultipleFactorials()
{
	/*
	 * Use decimal class to calculate big factorials then display their
	 * lengths
	 */
	const auto start = system_clock::now();
	/* Calculate multiple factorials */
	for (int i = 625; i <= 10000; i *= 2) {
		promise::resolved(i)
			->then(calcFactorial)
			->then(formatResult << start << i)
			->then(writeStr);
	}
	loop.join();
}

void calculateOneFactorial()
{
	/* Calculate one huge factorial */
	const auto tasks = cores >= 4 ? 4 : (cores + 1);
	const int x = 10001;
	vector<Promise<decimal>> partials;
	partials.reserve(tasks);
	const auto start = system_clock::now();
	for (remove_const<decltype(tasks)>::type i = 0; i < tasks; i++) {
		auto subrange = make_tuple(x, i + 1, tasks);
		partials.push_back(
			promise::resolved(subrange)
				->then(calcPartialFactorial));
	}
	promise::combine(partials)
		->then(seriesProduct)
		->then(formatResult << start << x)
		->then(writeStr);
	loop.join();
}

int main(int argc, char *argv[])
{
	calculateMultipleFactorials();
	calculateOneFactorial();
	this_thread::sleep_for(100ms);
	return assert.print(argc, argv);
}
