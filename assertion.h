#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <tuple>
#include <mutex>

namespace kaiu {

using namespace std;

class Assertions {
public:
	enum result { unknown, skipped, passed, failed };
	Assertions() = delete;
	Assertions(const Assertions&) = delete;
	Assertions(const vector<pair<const char *, const char *>>& strings);
	~Assertions();
	void set(const string& code, const result state, const string& note = "");
	void pass(const string& code, const string& note = "");
	void fail(const string& code, const string& note = "");
	void skip(const string& code, const string& note = "");
	void try_pass(const string& code, const string& note = "");
	template <typename T, typename U>
	void expect(const T& t, const U& u, const string& assertion, const string& note = "");
	int print(bool always);
	int print(const int argc, char const * const argv[]);
	void print_error();
private:
	using ensure_locked = const lock_guard<mutex>&;
	mutex mx;
	bool printed;
	const vector<pair<const char *, const char *>> strings;
	unordered_map<string, pair<result, string>> list;
	int _print(ensure_locked, bool always);
	void _set(ensure_locked, const string& code, const result state, const string& note);
	pair<result, string>& _get(ensure_locked, const string& code);
};

template <typename Actual, typename Expect>
void Assertions::expect(const Actual& actual, const Expect& expect, const string& assertion, const string& note)
{
	if (actual == expect) {
		pass(assertion, note);
	} else {
		fail(assertion, note);
//		cerr
//			<< "  Expect: " << expect << endl
//			<< "  Actual: " << actual << endl;
	}
}

}
