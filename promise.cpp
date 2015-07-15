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
#include "assertion.h"

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
	{ nullptr, "Promise combinators" },
	{ "PCR", "Resolves correctly" },
	{ "PCJ", "Rejects correctly" },
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
		->then<double>(
			[] (auto result) {
				assert.expect(result, 42, "IRPR");
				return 21.0;
			},
			[] (auto error) {
				assert.fail("IRPR");
				return 0;
			})
		->then<int>(
			[] (auto result) {
				assert.expect(result, 21.0, "IRCPR");
				return 69;
			},
			[] (auto error) {
				assert.fail("IRCPR");
				return -1;
			})
		->finally<int>(
			[] {
				return;
			})
		->then<int>(
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
		->finally<int>(
			[] {
				return;
			})
		->then<void *>(
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
		->then<Promise<string>>(
			[] (auto result) {
				Promise<string> promise;
				do_async_nonblock([promise] { promise->resolve("hi"); });
				return promise;
			})
		->then<Promise<int>>(
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
		->then<int>(
			[] (auto result) {
				assert.fail("AJPJ");
				return result;
			})
		->except<int>([] (auto error) {
			assert.pass("AJPJ");
			try {
				rethrow_exception(error);
			} catch (const exception& e) {
				assert.expect(e.what(), string{"failed"}, "EMPJH");
			}
			return 0;
		})
		->then<bool>(
			[] (auto result) {
				assert.expect(result, int(), "HJRPDV");
				return true;
			},
			[] (auto error) {
				assert.fail("HJRPDV");
				return true;
			})
		->finally<bool>(
			[] {
				assert.pass("FC");
				throw runtime_error("bye");
			})
		->then<char>(
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
			});
	/* Wait for chain to complete */
	unique_lock<mutex> lock(mx);
	cv.wait(lock, [&done] { return done; });
	/* Delay for thread to finalize after CV was set */
	this_thread::sleep_for(100ms);
}

void combine_test()
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

int main(int argc, char *argv[]) {
	auto printer = assert.printer();
	flow_test();
	combine_test();
	return assert.print();
}
#endif
