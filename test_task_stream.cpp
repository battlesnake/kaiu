#include <exception>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <queue>
#include <atomic>
#include "assertion.h"
#include "promise.h"
#include "promise_stream.h"
#include "task.h"
#include "task_stream.h"

using namespace std;
using namespace std::chrono;
using namespace std::literals;
using namespace kaiu;

Assertions assert({
	{ nullptr, "Concurrency" },
	{ "SYNCHRO", "Structured deadlock test" },
	{ "RSYNCHRO", "Random deadlock test" },
	{ nullptr, "Monads" },
	{ "MONAD", "Stateless" },
	{ "MONADS", "Stateful" }
});

class Trigger {
public:
	void wait()
	{
		unique_lock<mutex> lock(mx);
		cv.wait(lock, [this] { return fired; });
	}
	template <typename T>
	bool wait_for(T&& how_long)
	{
		unique_lock<mutex> lock(mx);
		cv.wait_for(lock, forward<T>(how_long), [this] { return fired; });
		return fired;
	}
	void fire()
	{
		unique_lock<mutex> lock(mx);
		fired = true;
		cv.notify_one();
	}
private:
	bool fired = false;
	mutex mx;
	condition_variable cv;
};

class Order {
public:
	enum Event { start, write, complete, return_, e_count };
	enum Which { consumer, producer, w_count };
	static constexpr char state_chars[4] = { 's', 'w', 'c', 'r' };
	Order(Event consumer_block_at, Event producer_block_at, Which release_first) :
		c(*this, consumer_block_at), p(*this, producer_block_at), rf(release_first)
			{ }
	void p_starting() { get(producer).maybe_block(start); }
	void p_writing() { get(producer).maybe_block(write); }
	void p_completing() { get(producer).maybe_block(complete); }
	void p_returning() { get(producer).maybe_block(return_); }
	void c_starting() { get(consumer).maybe_block(start); }
	void c_writing() { get(consumer).maybe_block(write); }
	void c_completing() { get(consumer).maybe_block(complete); }
	void c_returning() { get(consumer).maybe_block(return_); }
	bool trigger()
	{
		unique_lock<mutex> lock(mx);
		/* Not all orders will permit the lock condition to occur */
		ready.wait_for(lock, 10ms, [&] { return blocked == 2; });
		Which other = (rf == consumer) ? producer : consumer;
		get(rf).unblock();
		this_thread::sleep_for(10ms);
		get(other).unblock();
		done.wait_for(lock, 5s, [&] { return returned == 2; });
		if (returned != 2) {
			return false;
		}
		return true;
	}
	const string state() const
	{
		return string("p=") + p.get_state() + ", c=" + c.get_state();
	}
private:
	struct which_t {
	private:
		Order& order;
		Event block_at;
		Trigger trigger;
		char state = '-';
	public:
		which_t(Order& order, Event block_at) : order(order), block_at(block_at) { }
		void unblock()
		{
			trigger.fire();
		}
		void maybe_block(Event event)
		{
			state = Order::state_chars[event];
			if (event == block_at) {
				order.thread_blocked();
				trigger.wait();
			}
			if (event == return_) {
				order.thread_returned();
			}
			state = toupper(Order::state_chars[event]);
		}
		char get_state() const { return state; }
	};
	which_t& get(Which which)
	{
		return (which == consumer) ? c : p;
	}
	void thread_blocked()
	{
		lock_guard<mutex> lock(mx);
		if (++blocked == 2) {
			ready.notify_one();
		}
	}
	void thread_returned()
	{
		lock_guard<mutex> lock(mx);
		if (++returned == 2) {
			done.notify_one();
		}
	}
	which_t c, p;
	const Which rf;
	mutex mx;
	condition_variable ready;
	condition_variable done;
	int blocked = 0;
	int returned = 0;
};

void synchronization_order_test(const string name, Order& order, int idx)
{
	int locks;
	ParallelEventLoop loop({
		{ EventLoopPool::reactor, 6 },
	});
	auto get_remote_data = promise::task_stream<int, int>(
		{ [&] () {
			PromiseStream<int, int> stream;
			auto producer = [&locks, &order, stream] (EventLoop& loop) {
				order.p_writing();
				stream->write(42);
				order.p_completing();
				stream->resolve(372);
				order.p_returning();
			};
			order.p_starting();
			loop.push(EventLoopPool::reactor, producer);
			return stream;
		} },
		EventLoopPool::reactor,
		EventLoopPool::reactor,
		EventLoopPool::reactor);
	auto stream = get_remote_data(loop);
	auto binder = [&] (EventLoop&) {
		auto consumer = [&] (int data) {
			order.c_writing();
			if (data != 42) {
				assert.fail(name, "Wrong data");
			}
			return StreamAction::Continue;
		};
		auto verify = [&] (int result) {
			order.c_completing();
			if (result != 372) {
				assert.fail(name, "Wrong result");
			}
		};
		auto fail = [&] (exception_ptr) {
			assert.fail(name, "Promise stream rejected");
		};
		auto finalizer = [&] {
			order.c_returning();
		};
		order.c_starting();
		stream
			->stream(consumer)
			->then(verify, fail, finalizer);
	};
	loop.push(EventLoopPool::reactor, binder);
	this_thread::sleep_for(1ms);
	if (!order.trigger()) {
		assert.fail(name, "Probably deadlocked on test #" + to_string(idx) + ", state: " + order.state());
		std::abort();
	}
	loop.join();
}

void synchronization_test()
{
	int idx = 0;
	for (int which = Order::Which(0); which < Order::Which::w_count; which++) {
		for (int c_event = Order::Event(0); c_event < Order::Event::e_count; c_event++) {
			for (int p_event = Order::Event(0); p_event < Order::Event::e_count; p_event++) {
				Order order{Order::Event(c_event), Order::Event(p_event), Order::Which(which)};
				synchronization_order_test("SYNCHRO", order, idx++);
			}
		}
	}
	assert.try_pass("SYNCHRO");
}

void random_synchronization_test()
{
	const long count = 100000;
	printf("Attempting to trigger deadlock:\n");
	printf(" * If counter freezes, test has failed\n");
	printf(" * Tests where thousands column is odd do not provide state info\n\n");
	/* Logging state */
	class state_t {
	public:
		state_t(bool enabled) : enabled(enabled) { }
		void write(const char c) { ch[state_i++] = c; }
		const string get() const { return ch; }
	private:
		const bool enabled;
		char ch[9] = {32,32,32,32,32,32,32,32,0};
		int state_i{0};
	};
	atomic<bool> failed{false};
	for (int i = 1; i <= count && !failed; i++) {
		const bool log_state = (i / 1000 & 1) != 1;
		const long nmax = (i % 2) ? 1000 : 1;
		const long expect = (nmax * nmax + nmax) / 2;
		ParallelEventLoop loop{ {
			{ EventLoopPool::reactor, 2 }
		} };
		mutex consumer_mx, producer_mx;
		unique_lock<mutex> consumer_lock(consumer_mx), producer_lock(producer_mx);
		Trigger consumer_trig, producer_trig;
		state_t state(log_state);
		std::cout << "\r  Run " << i << " of " << count << "...";
		fflush(stdout);
		/* Mimic async read from some remote source */
		auto get_remote_data = [&producer_mx, &loop, nmax, expect, &state, &producer_trig] () -> PromiseStream<long, long> {
			PromiseStream<long, long> stream;
			auto producer_m = [stream, &producer_mx, nmax, expect, &state, &producer_trig] (EventLoop&) {
				/* Lock to block until this thread is triggered */
				unique_lock<mutex> lock(producer_mx);
				state.write('S');
				state.write('W');
				/* Write a load of messages to the stream */
				for (long n = 1; n <= nmax; n++) {
					stream->write(n);
				}
				state.write('W');
				/* Resolve the stream promise */
				stream->resolve(expect);
				state.write('R');
				producer_trig.fire();
			};
			/* Asynchronously write to the stream */
			loop.push(EventLoopPool::same, producer_m);
			/*
			 * Return promise stream object
			 *
			 * if we wrote to the stream synchronously, then the stream would
			 * contain all the data and also the result - streaming can't start
			 * until the promise stream has been returned!  In such a case, why
			 * not just use a promise instead?
			 */
			return stream;
		};
		auto get_remote_data_task = promise::task_stream(
			promise::StreamFactory<long, long>{get_remote_data},
			EventLoopPool::reactor,
			EventLoopPool::reactor,
			EventLoopPool::reactor);
		auto stream = get_remote_data_task(loop);
		/*
		 * Wrapped in async task so we can test with events bound before/after
		 * streaming started/resolved
		 */
		auto consumer_m = [stream, &consumer_mx, &failed, expect, nmax, &state, &consumer_trig] (EventLoop&) {
			/* Read data from stream and accumulate sum */
			auto consumer = [&failed, &state, nmax] (long& accum, const long value) {
				if (accum == 0) {
					state.write('w');
				}
				if (value == nmax) {
					state.write('w');
				}
				accum += value;
				return failed ? StreamAction::Stop : StreamAction::Continue;
			};
			/* Verify that calculated sum is correct */
			auto verify = [&failed] (const pair<long, long> res) {
				const long& result = res.first;
				const long& expect = res.second;
				if (expect != result) {
					assert.fail("RSYNCHRO", "Incorrect result");
					failed = true;
				}
			};
			auto handler = [&failed] (auto e) {
				assert.fail("RSYNCHRO", "Consumer chain failed");
				failed = true;
			};
			auto finalizer = [&state, &consumer_trig] {
				state.write('r');
				consumer_trig.fire();
			};
			/* Lock to block until this thread is triggered */
			lock_guard<mutex> lock(consumer_mx);
			state.write('s');
			stream
				->stream<long>(consumer)
				->then(verify, handler, finalizer);
		};
		loop.push(EventLoopPool::reactor, consumer_m);
		this_thread::sleep_for(10us);
		/* Alternate order of binding/producing */
		const char order = i / 7 % 4;
		/* Random delay between unlocking threads */
		const auto delay = duration<size_t, micro>(rand() % 100 + 1);
		switch (order) {
		case 0:
			consumer_lock.unlock();
			this_thread::sleep_for(delay);
			producer_lock.unlock();
			break;
		case 1:
			producer_lock.unlock();
			this_thread::sleep_for(delay);
			consumer_lock.unlock();
			break;
		case 2:
			producer_lock.unlock();
			consumer_lock.unlock();
			break;
		case 3:
			consumer_lock.unlock();
			producer_lock.unlock();
			break;
		}
		auto completed = producer_trig.wait_for(1s) && consumer_trig.wait_for(1s);
		if (!completed) {
			if (log_state) {
				assert.fail("RSYNCHRO", "Deadlock detected, state=" + state.get());
			} else {
				assert.fail("RSYNCHRO", "Deadlock detected, no state info available");
			}
			std::abort();
		}
		loop.join();
	}
	cout << "\r  Run " << count << " of " << count << " - DONE" << std::endl << std::endl;
	assert.try_pass("RSYNCHRO");
}

void concurrency_test()
{
	synchronization_test();
	//random_synchronization_test();
	assert.skip("RSYNCHRO", "Takes a really long time");
}

void operator_test()
{
	using namespace kaiu::promise::monads;
	using kaiu::promise::Factory;
	using kaiu::promise::StreamFactory;
	using kaiu::promise::StatefulConsumer;
	bool done;
	Factory<int, int> unit = [] (int value) {
		return promise::resolved(value);
	};
	StreamFactory<int, int, int> producer = [] (int result) {
		PromiseStream<int, int> stream;
		stream->write(42);
		stream->resolve(result);
		return stream;
	};
	{
		auto consumer = [] (int result) {
			return promise::resolved(StreamAction::Continue);
		};
		auto complete = [&] (int result) {
			if (result != 1) {
				assert.fail("MONAD", "Wrong result");
			}
			done = true;
		};
		auto failed = [] (exception_ptr) {
			assert.fail("MONAD", "Exception thrown");
		};
		auto chain = unit >>= producer >= consumer;
		done = false;
		chain(1)->then(complete, failed);
		if (done) {
			assert.try_pass("MONAD");
		}
	}
	{
		StatefulConsumer<int, int> consumer = [] (int& state, int result) {
			state = 2;
			return promise::resolved(StreamAction::Continue);
		};
		auto complete = [&] (pair<int, int> res) {
			auto& state = res.first;
			auto& result = res.second;
			if (result != 1 || state != 2) {
				assert.fail("MONADS", "Wrong result");
			}
			done = true;
		};
		auto failed = [] (exception_ptr) {
			assert.fail("MONADS", "Exception thrown");
		};
		auto chain = unit >>= producer >= consumer;
		done = false;
		chain(1)->then(complete, failed);
		if (done) {
			assert.try_pass("MONADS");
		}
	}
}

int main(int argc, char *argv[])
try {
	concurrency_test();
	operator_test();
	return assert.print(argc, argv);
} catch (...) {
	assert.print_error();
}
