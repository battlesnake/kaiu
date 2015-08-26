#include <stdexcept>
#include <iostream>
#include <sstream>
#include <algorithm>
#include "assertion.h"

namespace kaiu {

using namespace std;

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
}

Assertions::~Assertions()
{
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
	cerr << "Assertion " << code << " failed: " << endl;
}

void Assertions::skip(const string& code, const string& note)
{
	set(code, skipped, note);
}

void Assertions::_set(ensure_locked, const string& code, const result state, const string& note)
{
	try {
		auto& target = list.at(code);
		if (target.first == unknown) {
			target = make_pair(state, note);
		} else if (target.first == state && state == failed && target.second.empty()) {
			target = make_pair(state, note);
		} else {
			throw logic_error("Two results set for test '" + code + "'");
		}
	} catch (const out_of_range& e) {
		cerr << "Unknown assertion: " << code << endl;
	}
}

int Assertions::_print(ensure_locked, bool always)
{
	printed = true;
	stringstream out;
	stringstream fail_codes;
	unordered_map<result, size_t, hash<int>> count;
	for (const auto& string : strings) {
		const auto& code = string.first;
		const auto& title = string.second;
		if (code == nullptr) {
			out << endl
				<< "  \e[97m" << title << "\e[37m" << endl;
			continue;
		}
		const auto& result = list[code].first;
		const auto& note = list[code].second;
		count[result]++;
		if (result == passed) {
			out << "\e[32m    [PASS]\e[37;4m  " << title << "\e[24m";
		} else if (result == failed) {
			out << "\e[31m    [FAIL]\e[37m  " << title;
			fail_codes << "  " << code;
		} else if (result == skipped) {
			out << "\e[33m    [SKIP]\e[37m  " << title;
		} else if (result == unknown) {
			out << "\e[31m    [MISS]\e[37m  " << title;
			fail_codes << "  " << code;
		}
		if (!note.empty()) {
			out << " \e[35m" << note << "\e[37m";
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
	if (always || count[failed] + count[skipped] + count[unknown] > 0) {
		cout << out.rdbuf() << endl << endl;
	} else {
		cout << "\e[32m    [PASS]\e[37;4m  (all)\e[24m" << endl << endl;
	}
	return count[failed] + count[unknown];
}

}
