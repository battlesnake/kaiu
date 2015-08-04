#include <algorithm>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <list>
#include "assertion.h"
#include "promise.h"
#include "promise_stream.h"

using namespace std;
using namespace kaiu;

/*
 * TODO:
 *
 *   Test state-less stream<void> syntax
 *
 *   Test different orders of operations:
 *     stream/write/resolve (done)
 *     write/stream/resolve
 *     write/resolve/stream
 *     and also for s/resolve/reject/
 */

Assertions assert({
	{ nullptr, "Flow control" },
	{ "BASIC", "Data passes through in correct order and promise completes" },
	{ "DISCARD", "Discarded data is discarded" },
	{ "STOP", "Producer receives stop request" },
	{ "REJECT", "Rejected stream stops streaming then promise rejects" },
	{ nullptr, "Efficiency" },
	{ "NC", "Copy-free promise streams" },
});

void flow_test_continue()
{
	PromiseStream<int, vector<char>> basic_test_stream;
	/* Reads streamed data and builds a linked-list of buffers */
	auto consumer = [] (list<vector<char>>& state, vector<char>& value) -> StreamAction {
		state.emplace_back(move(value));
		return StreamAction::Continue;
	};
	/* When streaming is complete, this concatenates the buffers */
	auto concat = [] (pair<list<vector<char>>, int>& res) {
		if (res.second != 42) {
			assert.fail("BASIC", "Resolution value");
			return string();
		}
		list<vector<char>>& state = res.first;
		const auto length = accumulate(state.cbegin(), state.cend(), 0,
			[] (auto total, auto& vec) {
				return total + vec.size();
			});
		string s;
		s.reserve(length + 1);
		for_each(state.begin(), state.end(),
			[&s] (auto& vec) {
				s.append(vec.data(), vec.size());
				vector<char>().swap(vec);
			});
		this_thread::sleep_for(50ms);
		return s;
	};
	/* Test */
	auto verify = [] (string& s) {
		if (!s.empty()) {
			assert.expect(s, "Hello world!", "BASIC");
		}
	};
	/* Promise chain */
	basic_test_stream
		->stream<list<vector<char>>>(consumer)
		->then(concat)
		->then(verify);
	/* Write data to stream */
	basic_test_stream->write(vector<char>{ 'H', 'e', 'l', 'l', 'o' });
	basic_test_stream->write(vector<char>{ });
	basic_test_stream->write(vector<char>{ ' ' });
	basic_test_stream->write(vector<char>{ 'w', 'o', 'r', 'l', 'd' });
	basic_test_stream->write(vector<char>{ '!' });
	basic_test_stream->resolve(42);
}

void flow_test_discard()
{
	PromiseStream<int, vector<char>> basic_test_stream;
	/* Reads streamed data and builds a linked-list of buffers */
	auto consumer = [] (list<vector<char>>& state, vector<char>& value) {
		if (value.size() == 0) {
			return StreamAction::Discard;
		}
		state.emplace_back(move(value));
		return StreamAction::Continue;
	};
	/* When streaming is complete, this concatenates the buffers */
	auto concat = [] (pair<list<vector<char>>, int>& res) {
		if (res.second != 42) {
			assert.fail("DISCARD", "Resolution value");
			return string();
		}
		list<vector<char>>& state = res.first;
		const auto length = accumulate(state.cbegin(), state.cend(), 0,
			[] (auto total, auto& vec) {
				return total + vec.size();
			});
		string s;
		s.reserve(length + 1);
		for_each(state.begin(), state.end(),
			[&s] (auto& vec) {
				s.append(vec.data(), vec.size());
				vector<char>().swap(vec);
			});
		this_thread::sleep_for(50ms);
		return s;
	};
	/* Test */
	auto verify = [] (string& s) {
		if (!s.empty()) {
			assert.expect(s, "Hello", "DISCARD");
		}
	};
	/* Promise chain */
	basic_test_stream
		->stream<list<vector<char>>>(consumer)
		->then(concat)
		->then(verify);
	/* Write data to stream */
	basic_test_stream->write(vector<char>{ 'H', 'e', 'l', 'l', 'o' });
	basic_test_stream->write(vector<char>{ });
	basic_test_stream->write(vector<char>{ ' ' });
	basic_test_stream->write(vector<char>{ 'w', 'o', 'r', 'l', 'd' });
	basic_test_stream->write(vector<char>{ '!' });
	basic_test_stream->resolve(42);
}

void flow_test_stop()
{
	PromiseStream<int, vector<char>> basic_test_stream;
	/* Reads streamed data and builds a linked-list of buffers */
	auto consumer = [] (list<vector<char>>& state, vector<char>& value) {
		if (value.size() == 0) {
			return StreamAction::Stop;
		}
		state.emplace_back(move(value));
		return StreamAction::Continue;
	};
	/* When streaming is complete, this concatenates the buffers */
	auto concat = [] (pair<list<vector<char>>, int>& res) {
		if (res.second != 42) {
			assert.fail("STOP", "Resolution value");
			return string();
		}
		list<vector<char>>& state = res.first;
		const auto length = accumulate(state.cbegin(), state.cend(), 0,
			[] (auto total, auto& vec) {
				return total + vec.size();
			});
		string s;
		s.reserve(length + 1);
		for_each(state.begin(), state.end(),
			[&s] (auto& vec) {
				s.append(vec.data(), vec.size());
				vector<char>().swap(vec);
			});
		this_thread::sleep_for(50ms);
		return s;
	};
	/* Test */
	auto verify = [] (string& s) {
		if (!s.empty()) {
			assert.expect(s, "Hello", "STOP");
		}
	};
	/* Promise chain */
	basic_test_stream
		->stream<list<vector<char>>>(consumer)
		->then(concat)
		->then(verify);
	/* Write data to stream */
	if (basic_test_stream->is_stopping()) {
		assert.fail("STOP", "Unexpected stop request");
		basic_test_stream->reject("Failed");
	}
	basic_test_stream->write(vector<char>{ 'H', 'e', 'l', 'l', 'o' });
	if (basic_test_stream->is_stopping()) {
		assert.fail("STOP", "Unexpected stop request");
		basic_test_stream->reject("Failed");
	}
	basic_test_stream->write(vector<char>{ });
	if (!basic_test_stream->is_stopping()) {
		assert.fail("STOP", "Stop request not received");
		basic_test_stream->reject("Failed");
	} else {
		basic_test_stream->resolve(42);
	}
}

void flow_test_reject()
{
	PromiseStream<int, vector<char>> basic_test_stream;
	bool failed = false;
	/* Reads streamed data and builds a linked-list of buffers */
	auto consumer = [failed] (list<vector<char>>& state, vector<char>& value) {
		if (failed) {
			assert.fail("REJECT", "Data received after rejection");
		}
		state.emplace_back(move(value));
		return StreamAction::Continue;
	};
	auto next = [] (pair<list<vector<char>>, int>& res) {
		assert.fail("REJECT", "Promise stream resolved");
	};
	auto handler = [] (exception_ptr error) {
		assert.pass("REJECT");
	};
	/* Promise chain */
	basic_test_stream
		->stream<list<vector<char>>>(consumer)
		->then(next, handler);
	/* Write data to stream */
	basic_test_stream->write(vector<char>{ 'H', 'e', 'l', 'l', 'o' });
	basic_test_stream->write(vector<char>{ });
	basic_test_stream->write(vector<char>{ ' ' });
	basic_test_stream->reject("Magic smoke");
	failed = true;
	basic_test_stream->write(vector<char>{ 'o', 'o', 'p', 's' });
}

void flow_test()
{
	flow_test_continue();
	flow_test_discard();
	flow_test_stop();
	flow_test_reject();
}

void efficiency_test()
{
	using Stone = unique_ptr<int>;
	auto make_stone = [] (const int x) {
		return make_unique<int>(x);
	};
	PromiseStream<Stone, Stone> test_stream;
	auto consumer = [] (Stone& state, Stone& datum) {
		*state += *datum;
		return StreamAction::Continue;
	};
	auto next = [] (pair<Stone, Stone>& res) {
		Stone& state = res.first;
		Stone& result = res.second;
		assert.expect(*state, *result, "NC");
	};
	test_stream
		->stream<Stone>(consumer, make_stone(0))
		->then(next);
	test_stream->write(make_stone(1));
	test_stream->write(make_stone(2));
	test_stream->write(make_stone(3));
	test_stream->resolve(make_stone(6));
}

int main(int argc, char *argv[])
try {
	flow_test();
	efficiency_test();
	return assert.print(argc, argv);
} catch (...) {
	assert.print_error();
}
