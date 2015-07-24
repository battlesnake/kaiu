#include <exception>
#include <stdexcept>
#include <thread>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include "assertion.h"
#include "promise.h"

using namespace std;
using namespace mark;

Assertions assert({
	{ nullptr, "Immediates" },
	{ "IRPR", "Immediately resolved promise resolves" },
	{ "IRCPR", "Immediately resolved chained promise resolves" },
	{ nullptr, "Transparent finalizers" },
	{ "RVPF", "Resolved value passes through finally() stage unaltered" },
	{ "JVPF", "Rejection passes through finally() stage unaltered" },
	{ nullptr, "Asynchronous promises" },
	{ "ARPR", "Asynchronously resolved promise resolved" },
	{ "AJPJ", "Asynchronously rejected promise rejected" },
	{ nullptr, "Exception handler behaviour" },
	{ "EMPJH", "Exception message passes to rejection handler" },
	{ "HJRPDV", "Handled rejection results in resolved promise with default value if none specified by handler" },
	{ nullptr, "Finalizer behaviour" },
	{ "FC", "Finalizer called" },
	{ "EFJP", "Exception in finalizer results in rejected promise" },
	{ "FCEF", "Finally handler called even on exception in previous finally handler" },
	{ nullptr, "Promise combinator (heterogenous)" },
	{ "PCR", "Resolves correctly" },
	{ "PCJ", "Rejects correctly" },
	{ nullptr, "Promise combinator (homogenous)" },
	{ "VCR", "Resolves correctly" },
	{ "VCJ", "Rejects correctly" },
});

void do_async_nonblock(function<void()> op)
{
	thread(op).detach();
}

void flow_test() {
	mutex mx;
	condition_variable cv;
	volatile bool done = false;
	promise::resolved(42)
		->then(
			[] (auto result) {
				assert.expect(result, 42, "IRPR");
				return 21.0;
			},
			[] (auto error) {
				assert.fail("IRPR");
				return 0;
			})
		->then(
			[] (auto result) {
				assert.expect(result, 21.0, "IRCPR");
				return 69;
			},
			[] (auto error) {
				assert.fail("IRCPR");
				return -1;
			})
		->finally(
			[] {
				return;
			})
		->then(
			[] (auto result) {
				assert.expect(result, 69, "RVPF");
				throw runtime_error("oops");
				return -1;
			},
			[] (auto error) {
				assert.fail("RVPF");
				throw runtime_error("oops");
				return -1;
			})
		->finally(
			[] {
				return;
			})
		->then(
			[] (auto result) {
				assert.fail("JVPF");
				return nullptr;
			},
			[] (auto error) {
				try {
					rethrow_exception(error);
				} catch (const exception& e) {
					assert.expect(string(e.what()), string("oops"), "JVPF");
				}
				return nullptr;
			})
		->then(
			[] (auto result) {
				Promise<string> promise;
				do_async_nonblock([promise] { promise->resolve("hi"); });
				return promise;
			})
		->then(
			[] (auto result) {
				assert.expect(result, "hi", "ARPR");
				Promise<int> promise;
				do_async_nonblock([promise] { promise->reject("failed"); });
				return promise;
			},
			[] (auto error) {
				assert.fail("ARPR");
				return Promise<int>(-1);
			})
		->then(
			[] (auto result) {
				assert.fail("AJPJ");
				return result;
			})
		->except([] (auto error) {
			assert.pass("AJPJ");
			try {
				rethrow_exception(error);
			} catch (const exception& e) {
				assert.expect(e.what(), string{"failed"}, "EMPJH");
			}
			return 0;
		})
		->then(
			[] (auto result) {
				assert.expect(result, int(), "HJRPDV");
				return true;
			},
			[] (auto error) {
				assert.fail("HJRPDV");
				return true;
			})
		->finally(
			[] {
				assert.pass("FC");
				throw runtime_error("bye");
			})
		->then(
			[] (auto result) {
				assert.fail("EFJP");
				return 0;
			},
			[] (auto error) {
				assert.pass("EFJP");
				return 0;
			},
			[] {
				assert.pass("FCEF");
			})
		->finally(
			[&done, &cv] {
				done = true;
				cv.notify_one();
			})
		->finish();
	/* Wait for chain to complete */
	unique_lock<mutex> lock(mx);
	cv.wait(lock, [&done] { return done; });
	/* Delay for thread to finalize after CV was set */
	this_thread::sleep_for(100ms);
}

void static_combine_test()
{
	promise::combine(
		promise::resolved(int(2)),
		promise::resolved(float(3.1f)),
		promise::resolved(string("hello"))
	)->then(
		[] (const auto& result) {
			assert.expect(
				get<0>(result) == 2 &&
				get<1>(result) == 3.1f &&
				get<2>(result) == string("hello"),
				true,
				"PCR");
		},
		[] (const auto& error) {
			assert.fail("PCR");
		});
	promise::combine(
		promise::resolved(int(2)),
		promise::rejected<float>("Kartuliõis!"),
		promise::resolved(string("hello"))
	)->then(
		[] (const auto& result) {
			assert.fail("PCJ");
		},
		[] (const auto& error) {
			try {
				rethrow_exception(error);
			} catch (const runtime_error& e) {
				assert.expect(e.what(), string("Kartuliõis!"), "PCJ");
			}
		});
}

void dynamic_combine_test()
{
	const size_t count = 10;
	function<size_t(const size_t)> func = [] (const size_t i) { return i; };
	auto fac = promise::factory(func);
	vector<Promise<size_t>> seq;
	seq.reserve(count);
	for (size_t i = 0; i < count; i++) {
		seq.emplace_back(fac(i));
	}
	promise::combine(seq)
		->then(
			[] (const auto& result) {
				bool pass = true;
				for (size_t i = 0; i < count; i++) {
					pass = pass && result[i] == i;
				}
				assert.expect(pass, true, "VCR");
			},
			[] (const auto& error) {
				assert.fail("VCR");
			});
	seq.resize(0);
	seq.emplace_back(promise::rejected<size_t>("These aren't the droids you're looking for"));
	for (size_t i = 1; i < count; i++) {
		seq.emplace_back(fac(i));
	}
	promise::combine(seq)
		->then(
			[] (const auto& result) {
				assert.fail("VCJ");
			},
			[] (const auto& error) {
				assert.pass("VCJ");
			});
}

int main(int argc, char *argv[]) {
	auto printer = assert.printer();
	flow_test();
	static_combine_test();
	dynamic_combine_test();
	return assert.print(argc, argv);
}
