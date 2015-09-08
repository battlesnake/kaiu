#include <exception>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "assertion.h"
#include "promise.h"

using namespace std;
using namespace kaiu;

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
	{ nullptr, "Efficiency" },
	{ "NC", "Copy-free promise chaining" },
	{ "NCP", "Copy-free heterogenous combinator" },
	{ "NCV", "Copy-free homogenous combinator" },
	{ nullptr, "Compiler handles optional arguments correctly (statically checked)" },
	{ "OATH", "Then: omit 'handler'" },
	{ "OATF", "Then: omit 'finalizer'" },
	{ nullptr, "Promise monad" },
	{ "MONAD1", "Chain #1" },
	{ "MONAD2", "Chain #2" },
	{ "MONADE1", "Exception handling #1" },
	{ "MONADE2", "Exception handling #2" },
});

void do_async_nonblock(function<void()> op)
{
	auto func = [op] () noexcept {
		try {
			op();
		} catch (...) {
			assert.print_error();
		}
	};
	thread(func).detach();
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
				return promise::resolved<int>(-1);
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
		[] (const auto result) {
			assert.expect(
				get<0>(result) == 2 &&
				get<1>(result) == 3.1f &&
				get<2>(result) == string("hello"),
				true,
				"PCR");
		},
		[] (const auto error) {
			assert.fail("PCR");
		});
	promise::combine(
		promise::resolved(int(2)),
		promise::rejected<float>("Kartuliõis!"),
		promise::resolved(string("hello"))
	)->then(
		[] (const auto result) {
			assert.fail("PCJ");
		},
		[] (const auto error) {
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
			[] (const auto result) {
				bool pass = true;
				for (size_t i = 0; i < count; i++) {
					pass = pass && result[i] == i;
				}
				assert.expect(pass, true, "VCR");
			},
			[] (const auto error) {
				assert.fail("VCR");
			});
	seq.resize(0);
	seq.emplace_back(promise::rejected<size_t>("These aren't the droids you're looking for"));
	for (size_t i = 1; i < count; i++) {
		seq.emplace_back(fac(i));
	}
	promise::combine(seq)
		->then(
			[] (const auto result) {
				assert.fail("VCJ");
			},
			[] (const auto error) {
				assert.pass("VCJ");
			});
}

void efficiency_test()
{
	{
		promise::resolved(true)
			->then([] (bool result) {
				return make_unique<int>(42);
			})
			->except([] (auto e) {
				assert.fail("NC");
				rethrow_exception(e);
				return unique_ptr<int>(nullptr);
			})
			->finally([] () {
			})
			->then([] (auto ptr) {
				assert.expect(*ptr, 42, "NC");
			});
	}
	{
		promise::combine(
			promise::resolved(make_unique<int>(1)),
			promise::resolved(make_unique<int>(2)))
			->then([] (auto result) {
				assert.expect(
					*get<0>(result) == 1 && *get<1>(result) == 2, true,
					"NCP");
			});
	}
	{
		vector<Promise<unique_ptr<int>>> v;
		v.emplace_back(promise::resolved(make_unique<int>(1)));
		v.emplace_back(promise::resolved(make_unique<int>(2)));
		promise::combine(v)
			->then([] (auto result) {
				assert.expect(
					result.size() == 2 &&
					*result[0] == 1 && *result[1] == 2, true,
					"NCV");
			});
	}
}

void static_checks()
{
	promise::resolved(42)
		->then(
			[] (auto result) {
				return 21;
			},
			nullptr,
			[] () {
			})
		->then(
			[] (int result) {
				assert.expect(result, 21, "OATH");
			},
			[] (exception_ptr) {
				assert.fail("OATH");
			});
	promise::resolved(42)
		->then(
			[] (auto result) {
				return 21;
			},
			[] (exception_ptr) {
				return 42;
			},
			nullptr)
		->then(
			[] (int result) {
				assert.expect(result, 21, "OATF");
			},
			[] (exception_ptr) {
				assert.fail("OATF");
			});
}

/* Easier than planting breakpoints in gdb */
void do_segfault()
{
	*(int *)0 = 42;
}

void monadic_test()
{
	using promise::Factory;
	using namespace promise::monads;
	struct monad_test {
		monad_test(const string name) : complete(false), name(name) { }
		monad_test() : monad_test(string{}) { }
		void fail(const string msg) const { assert.fail(name, msg); }
		void pass() const { assert.try_pass(name); }
		void end()
		{
			if (name.empty()) {
				return;
			}
			if (complete) {
				pass();
			} else {
				fail("Test not completed");
			}
			name = {};
		}
		void completed() { complete = true; }
	private:
		bool complete;
		string name;
	};
	static monad_test this_test;
	struct begin_test {
		begin_test(const string name) { this_test = monad_test(name); }
		void end() { this_test.end(); }
		~begin_test() { end(); }
	};
	/* Basic operations */
	Factory<int, int> sqr {[] (int x) {
		return promise::resolved(x * x);
	}};
	auto add = [] (int addend) {
		return Factory<int, int> {[=] (int x) {
			return promise::resolved(x + addend);
		}};
	};
	auto div{[] (int divisor) {
		return Factory<int, int> {[=] (int x) {
			if (divisor == 0) {
				throw runtime_error("Division by zero");
			}
			return promise::resolved(x / divisor);
		}};
	}};
	/* Assert current value in chain */
	auto test = [] (int expect) -> Factory<int, int> {
		return [expect] (int x) {
			if (x != expect) {
				this_test.fail("Test result was incorrect");
			}
			this_test.completed();
			return promise::resolved(x);
		};
	};
	/* Should never be called */
	Factory<int, int> test_fail {[] (int) {
		this_test.fail("Catch block not triggered by exception");
		return promise::resolved(0);
	}};
	/* Unexpected exception */
	Factory<int, exception_ptr> catcher {[] (exception_ptr) {
		this_test.fail("Unexpected exception thrown");
		throw logic_error("Promise flow control broken");
		return promise::resolved(0);
	}};
	/* Rethrow, testing (delete this) */
	Factory<int, exception_ptr> rethrow {[] (exception_ptr err) {
		rethrow_exception(err);
		return promise::resolved(0);
	}};
	/* Consume error, return new value */
	auto must_catch = [] (int value) -> Factory<int, exception_ptr> {
		return [value] (exception_ptr) {
			return promise::resolved(value);
		};
	};
	Factory<int, int> then_segfault {[] (int value) {
		do_segfault();
		return promise::resolved(0);
	}};
	Factory<int, exception_ptr> except_segfault {[] (exception_ptr) {
		do_segfault();
		return promise::resolved(0);
	}};
	function<void()> finally_segfault {[] () {
		do_segfault();
	}};
	auto eat_error = [] (exception_ptr) { };
	auto segfault = then_segfault/except_segfault/finally_segfault;
	{
		auto t = begin_test("MONAD1");
		auto chain = sqr >>= test(169)/catcher >>= add(31) >>= div(20) >>= test(10)/catcher;
		chain(13)->except(eat_error);
		t.end();
	}
	{
		auto t = begin_test("MONAD2");
		auto chain = sqr >>= test(3600)/catcher >>= add(496) >>= div(256) >>= test(16)/catcher;
		chain(60)->except(eat_error);
		t.end();
	}
	{
		auto t = begin_test("MONADE1");
		/*
		 * segfault triggers here, but not when shifted right one place - so the
		 * rejected promise is not being propagated along the monad chain.
		 */
		auto chain = add(100) >>= div(0)/catcher >>= test_fail/rethrow >>=
			test_fail/must_catch(69) >>= div(3)/catcher >>= test(23)/catcher;
		chain(1);
		t.end();
	}
	{
		auto t = begin_test("MONADE2");
		auto chain = add(100) >>= div(0)/catcher >>= test_fail >>=
			nullptr/must_catch(69) >>= div(3)/catcher/nullptr >>=
			nullptr/catcher >>= test(23)/catcher;
		chain(1);
		t.end();
	}
}

int main(int argc, char *argv[])
try {
	flow_test();
	static_combine_test();
	dynamic_combine_test();
	efficiency_test();
	static_checks();
	monadic_test();
	return assert.print(argc, argv);
} catch (...) {
	assert.print_error();
}
