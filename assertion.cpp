#include <stdexcept>
#include <iostream>
#include "assertion.h"

namespace mark {

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
	if (!printed) {
		print();
	}
}

int Assertions::print()
{
	printed = true;
	unordered_map<State, size_t, hash<int>> count;
	for (const auto& string : strings) {
		const auto& code = string.first;
		const auto& title = string.second;
		if (code == nullptr) {
			cout << endl
				<< "  \e[97m" << title << "\e[37m" << endl;
			continue;
		}
		const auto& result = list[code].first;
		const auto& note = list[code].second;
		count[result]++;
		if (result == passed) {
			cout << "\e[32m    [PASS]\e[37;4m  " << title << "\e[24m";
		} else if (result == failed) {
			cout << "\e[31m    [FAIL]\e[37m  " << title;
		} else if (result == skipped) {
			cout << "\e[33m    [SKIP]\e[37m  " << title;
		} else if (result == unknown) {
			cout << "\e[31m    [MISS]\e[37m  " << title;
		}
		if (!note.empty()) {
			cout << " \e[35m" << note << "\e[37m";
		}
		cout << endl;
	}
	cout << endl
		<< "     Passed: " << count[passed] << endl
		<< "     Failed: " << count[failed] << endl;
	if (count[skipped]) {
		cout << "    Skipped: " << count[skipped] << endl;
	}
	if (count[unknown]) {
		cout << "     Missed: " << count[unknown] << endl;
	}
	cout << endl;
	return count[failed] + count[unknown];
}

void Assertions::set(const string& code, const State state, const string& note)
{
	auto& target = list.at(code);
	if (target.first == unknown) {
		target = make_pair(state, note);
	} else {
		throw logic_error("Two results set for test '" + code + "'");
	}
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

}
