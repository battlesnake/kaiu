#include <stdexcept>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <signal.h>
#include <stdio.h>
#include "assertion.h"

namespace kaiu {

using namespace std;
using namespace std::chrono;

Assertions *assertions;
struct sigaction new_sigsegv;

static void sigsegv_handler(int signo, siginfo_t *info, void *context)
{
	if (assertions) {
		printf("Segfault detected, attempting to print current state of tests\n");
		fflush(stdout);
		assertions->print(true);
	} else {
		printf("Segfault detected\n");
	}
	exit(255);
}

static void sigsegv_attach(Assertions& ass)
{
	if (!assertions) {
		sigemptyset(&new_sigsegv.sa_mask);
		new_sigsegv.sa_flags = SA_SIGINFO;
		new_sigsegv.sa_sigaction = &sigsegv_handler;
		if (sigaction(SIGSEGV, &new_sigsegv, nullptr) == 0) {
			assertions = &ass;
		} else {
			printf("Failed to install SIGSEGV handler\n");
		}
	}
}

static void sigsegv_detach(Assertions& ass)
{
	if (assertions == &ass) {
		assertions = nullptr;
	}
}

Assertions::Assertions(const vector<pair<const char *, const char *>>& strings)
	: printed(false), strings(strings)
{
	for (auto const& test : strings) {
		const auto& code = test.first;
		if (code == nullptr) {
			continue;
		}
		if (!list.emplace(make_pair(code, make_pair(unknown, ""))).second) {
			throw logic_error("Duplicate test: '" + string(code) + "'");
		}
	}
	sigsegv_attach(*this);
}

Assertions::~Assertions()
{
	sigsegv_detach(*this);
	lock_guard<mutex> lock(mx);
	if (!printed) {
		_print(lock, false);
	}
}

int Assertions::print(bool always)
{
	lock_guard<mutex> lock(mx);
	return _print(lock, always);
}

int Assertions::print(const int argc, char const * const argv[])
{
	lock_guard<mutex> lock(mx);
	bool quiet = any_of(argv + 1, argv + argc,
		[] (const auto s) {
			return string(s) == "--test-silent-if-perfect";
		});
	return _print(lock, !quiet);
}

void Assertions::print_error()
{
	exception_ptr ptr = current_exception();
	if (!ptr) {
		return;
	}
	try {
		rethrow_exception(ptr);
	} catch (const logic_error& error) {
		cout << "\x1b[1;31mLogic error: \x1b[22m" << error.what() << "\x1b[37m" << endl;
	} catch (const runtime_error& error) {
		cout << "\x1b[1;31mRuntime error: \x1b[22m" << error.what() << "\x1b[37m" << endl;
	} catch (const exception& error) {
		cout << "\x1b[1;31mException: \x1b[22m" << error.what() << "\x1b[37m" << endl;
	} catch(...) {
		cout << "\x1b[1;31mError of unknown type\x1b[37m" << endl;
	}
	print(true);
}

void Assertions::set(const string& code, const result state, const string& note)
{
	lock_guard<mutex> lock(mx);
	_set(lock, code, state, note);
}

void Assertions::pass(const string& code, const string& note)
{
	set(code, passed, note);
}

void Assertions::fail(const string& code, const string& note)
{
	set(code, failed, note);
}

void Assertions::skip(const string& code, const string& note)
{
	set(code, skipped, note);
}

void Assertions::try_pass(const string& code, const string& note)
{
	lock_guard<mutex> lock(mx);
	auto& target = _get(lock, code);
	if (target.first == unknown) {
		_set(lock, code, passed, note);
	}
}

auto Assertions::_get(ensure_locked, const string& code)
	-> pair<result, string>&
try {
	return list.at(code);
} catch (const out_of_range& e) {
	cout << "\x1b[1;31mUnknown assertion: \x1b[22m" << code << "\x1b[37m" << endl;
	throw;
}

void Assertions::_set(ensure_locked lock, const string& code, const result state, const string& note)
{
	auto& target = _get(lock, code);
	if (target.first == unknown) {
		target = make_pair(state, note);
	} else if (target.first == failed) {
		if (state == failed) {
			if (target.second.empty()) {
				target = make_pair(state, note);
			} else {
				target.second += " \x1b[1malso\x1b[22m " + note;
			}
		}
	} else {
		throw logic_error("Two results set for test '" + code + "'");
	}
}

int Assertions::_print(ensure_locked, bool always)
{
	const auto end_time = steady_clock::now();
	duration<float, milli> msecs = end_time - start_time;
	printed = true;
	stringstream out;
	stringstream fail_codes;
	unordered_map<result, size_t, hash<int>> count;
	for (const auto& string : strings) {
		const auto& code = string.first;
		const auto& title = string.second;
		if (code == nullptr) {
			out << endl
				<< "  \x1b[97m" << title << "\x1b[37m" << endl;
			continue;
		}
		const auto& result = list[code].first;
		const auto& note = list[code].second;
		count[result]++;
		if (result == passed) {
			out << "\x1b[32m    [PASS]\x1b[37;4m  " << title << "\x1b[24m";
		} else if (result == failed) {
			out << "\x1b[31m    [FAIL]\x1b[37m  " << title;
			fail_codes << "  " << code;
		} else if (result == skipped) {
			out << "\x1b[33m    [SKIP]\x1b[37m  " << title;
		} else if (result == unknown) {
			out << "\x1b[31m    [MISS]\x1b[37m  " << title;
			fail_codes << "  " << code;
		}
		if (!note.empty()) {
			out << " \x1b[35m" << note << "\x1b[37m";
		}
		out << endl;
	}
	out << endl
		<< "     Passed: " << count[passed] << endl
		<< "     Failed: " << count[failed] << fail_codes.rdbuf() << endl;
	if (count[skipped]) {
		out << "    Skipped: " << count[skipped] << endl;
	}
	if (count[unknown]) {
		out << "     Missed: " << count[unknown] << endl;
	}
	out << "    Elapsed: " << msecs.count() << "ms" << endl;
	if (always || count[failed] + count[skipped] + count[unknown] > 0) {
		cout << out.rdbuf() << endl;
	} else {
		cout << "\x1b[32m    [PASS]\x1b[37;4m  (all)\x1b[24m \x1b[35m" << msecs.count() << "ms" << "\x1b[37m" << endl;
	}
	return count[failed] + count[unknown];
}

}
