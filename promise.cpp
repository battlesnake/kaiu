#include "promise.h"

namespace mark {

using namespace std;

/*** PromiseInternalBase ***/

PromiseInternalBase::PromiseInternalBase()
{
}

PromiseInternalBase::~PromiseInternalBase() //noexcept(false)
{
	UserspaceSpinlock lock(state_lock);
	if (state == promise_state::completed) {
		return;
	}
	if (state == promise_state::pending && !callbacks_assigned) {
		return;
	}
	//on_resolve = nullptr;
	//on_reject = nullptr;
	//throw logic_error("Promise destructor called on uncompleted promise");
}

PromiseInternalBase::PromiseInternalBase(const nullptr_t dummy, exception_ptr error) :
	error(error)
{
	UserspaceSpinlock lock(state_lock);
	rejected();
}

PromiseInternalBase::PromiseInternalBase(const nullptr_t dummy, const string& error) :
	error(make_exception_ptr(runtime_error(error)))
{
	UserspaceSpinlock lock(state_lock);
	rejected();
}

void PromiseInternalBase::ensure_is_still_pending() const
{
	if (state != promise_state::pending) {
		throw logic_error("Attempted to resolve/reject an already resolved/rejected promise");
	}
}

void PromiseInternalBase::assigned_callbacks()
{
	callbacks_assigned = true;
	if (state == promise_state::rejected) {
		rejected();
	} else if (state == promise_state::resolved) {
		resolved();
	}
}

void PromiseInternalBase::reject(exception_ptr error)
{
	ensure_is_still_pending();
	UserspaceSpinlock lock(state_lock);
	ensure_is_still_pending();
	this->error = error;
	rejected();
}

void PromiseInternalBase::reject(const string& error)
{
	reject(make_exception_ptr(runtime_error(error)));
}

void PromiseInternalBase::resolved()
{
	/* Only called from within state_lock spinlock */
	state = promise_state::resolved;
	if (callbacks_assigned) {
		on_resolve();
		completed_and_called();
	}
}

void PromiseInternalBase::rejected()
{
	/* Only called from within state_lock spinlock */
	state = promise_state::rejected;
	if (callbacks_assigned) {
		on_reject();
		completed_and_called();
	}
}

void PromiseInternalBase::completed_and_called()
{
	/* Only called from within state_lock spinlock */
	if (state == promise_state::pending) {
		throw logic_error("Attempted to mark uncompleted promise as complete");
	}
	state = promise_state::completed;
	/*
	 * The callbacks may hold references to this promise's container, via
	 * closure.  Let's break circular references so we don't memory leak...
	 */
	on_resolve = nullptr;
	on_reject = nullptr;
	error = exception_ptr{};
}

void PromiseInternalBase::ensure_is_unbound() const
{
	if (callbacks_assigned) {
		throw logic_error("Attempted to double-bind to promise");
	}
}

namespace promise {

Promise<nullptr_t> begin_chain()
{
	return Promise<nullptr_t>{nullptr};
}

}

}

#ifdef test_promise
#include <exception>
#include <stdexcept>
#include <thread>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <map>
#include <vector>
#include <utility>

using namespace std;
using namespace mark;

enum assertion_state { unknown, passed, failed };

const vector<pair<const char *, const char *>> assertion_strings{
	{ "IRPR", "Immediately resolved promise resolves" },
	{ "IRCPR", "Immediately resolved chained promise resolves" },
	{ "RVPF", "Resolved value passes through finally() stage" },
	{ "JVPF", "Rejection passes through finally() stage" },
	{ "ROPR", "Resolved promise<object> resolved correctly" },
	{ "ARPR", "Asynchronously resolved promise resolved" },
	{ "AJPJ", "Asynchronously rejected promise rejected" },
	{ "EMPJH", "Exception message passes to rejection handler" },
	{ "HJRPDV", "Handled rejection results in resolved promise with default value if none specified by handler" },
	{ "FC", "Finalizer called" },
	{ "EFJP", "Exception in finalizer results in rejected promise" },
	{ "FCEF", "Finally handler called even on exception in previous finally handler" },
	{ "DONE", "Done" }
};

map<string, assertion_state> assertions;

void build_assertions()
{
	for (auto const& strings : assertion_strings) {
		assertions.emplace(make_pair(strings.first, unknown));
	}
}

void assert_pass(const string assertion)
{
	if (assertions[assertion] == failed) {
		return;
	}
	assertions[assertion] = passed;
}

void assert_fail(const string assertion)
{
	assertions[assertion] = failed;
}

template <typename T, typename U>
void assert(const T t, const U u, const string assertion)
{
	if (t == u) {
		assert_pass(assertion);
	} else {
		assert_fail(assertion);
	}
}

void do_async_nonblock(function<void()> op)
{
	thread(op).detach();
}

void run_tests();

int main(int argc, char *argv[]) {
	try {
		run_tests();
	} catch (const exception& e) {
		cout << "EXCEPTION: " << e.what() << endl;
	}
	map<assertion_state, size_t> count{
		{ unknown, 0 },
		{ passed, 0 },
		{ failed, 0 }
	};
	for (const auto& strings : assertion_strings) {
		const auto& title = strings.second;
		const auto& result = assertions[strings.first];
		count[result]++;
		if (result == passed) {
			cout << "\e[32m  [PASS]\e[0m  " << title << endl;
		} else if (result == failed) {
			cout << "\e[31m  [FAIL]\e[0m  " << title << endl;
		} else if (result == unknown) {
			cout << "\e[33m  [MISS]\e[0m  " << title << endl;
		}
	}
	cout << endl
		<< "\t        Passed: " << count[passed] << endl
		<< "\t        Failed: " << count[failed] << endl
		<< "\tSkipped/missed: " << count[unknown] << endl;
}

void run_tests() {
	mutex mx;
	condition_variable cv;
	volatile bool done = false;
	promise::resolved(42)
		->then<double>(
			[] (auto result) {
				assert(result, 42, "IRPR");
				return 21.0;
			},
			[] (auto error) {
				assert_fail("IRPR");
				return 0;
			})
		->then<int>(
			[] (auto result) {
				assert(result, 21.0, "IRCPR");
				return 69;
			},
			[] (auto error) {
				assert_fail("IRCPR");
				return -1;
			})
		->finally<int>(
			[] {
				return;
			})
		->then<int>(
			[] (auto result) {
				assert(result, 69, "RVPF");
				throw runtime_error("oops");
				return -1;
			},
			[] (auto error) {
				assert_fail("RVPF");
				throw runtime_error("oops");
				return -1;
			})
		->finally<int>(
			[] {
				return;
			})
		->then<string>(
			[] (auto result) {
				assert_fail("JVPF");
				return "hello";
			},
			[] (auto error) {
				try {
					rethrow_exception(error);
				} catch (const exception& e) {
					assert(string(e.what()), string("oops"), "JVPF");
				}
				return "hello";
			})
		->then<Promise<string>>(
			[] (auto result) {
				assert(result, "hello", "ROPR");
				Promise<string> promise;
				do_async_nonblock([promise] { promise->resolve("hi"); });
				return promise;
			},
			[] (auto error) {
				assert_fail("ROPR");
				return Promise<string>(nullptr);
			})
		->then<Promise<int>>(
			[] (auto result) {
				assert(result, "hi", "ARPR");
				Promise<int> promise;
				do_async_nonblock([promise] { promise->reject("failed"); });
				return promise;
			},
			[] (auto error) {
				assert_fail("ARPR");
				return Promise<int>(-1);
			})
		->then<int>(
			[] (auto result) {
				assert_fail("AJPJ");
				return result;
			})
		->except<int>([] (auto error) {
			assert_pass("AJPJ");
			try {
				rethrow_exception(error);
			} catch (const exception& e) {
				assert(e.what(), string{"failed"}, "EMPJH");
			}
			return 0;
		})
		->then<bool>(
			[] (auto result) {
				assert(result, int(), "HJRPDV");
				return true;
			},
			[] (auto error) {
				assert_fail("HJRPDV");
				return true;
			})
		->finally<bool>(
			[] {
				assert_pass("FC");
				throw runtime_error("bye");
			})
		->then<char>(
			[] (auto result) {
				assert_fail("EFJP");
				return 0;
			},
			[] (auto error) {
				assert_pass("EFJP");
				return 0;
			},
			[] {
				assert_pass("FCEF");
			})
		->finally(
			[&done, &cv] {
				assert_pass("DONE");
				done = true;
				cv.notify_one();
			});
	/* Wait for chain to complete */
	unique_lock<mutex> lock(mx);
	cv.wait(lock, [&done] { return done; });
	/* Delay for thread to finalize after CV was set */
	this_thread::sleep_for(100ms);
}
#endif
