#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <tuple>

namespace mark {

using namespace std;

class Assertions {
public:
	enum State { unknown, skipped, passed, failed };
	Assertions() = delete;
	Assertions(const Assertions&) = delete;
	Assertions(const vector<pair<const char *, const char *>>& strings);
	~Assertions();
	void set(const string& code, const State state, const string& note = "");
	void pass(const string& code, const string& note = "");
	void fail(const string& code, const string& note = "");
	void skip(const string& code, const string& note = "");
	template <typename T, typename U>
	void expect(const T& t, const U& u, const string& assertion, const string& note = "");
	int print();
	/* Prints on destruction if no print has happened yet - a scope guard */
	class Printer {
	public:
		Printer() = delete;
		Printer(Assertions& assert) : assert(assert) { };
		~Printer() { if (!assert.printed) { assert.print(); } };
	private:
		Assertions& assert;
	};
	Printer printer() { return Printer(*this); };
private:
	bool printed;
	const vector<pair<const char *, const char *>> strings;
	unordered_map<string, pair<State, string>> list;
};

template <typename T, typename U>
void Assertions::expect(const T& t, const U& u, const string& assertion, const string& note)
{
	if (t == u) {
		pass(assertion, note);
	} else {
		fail(assertion, note);
	}
}

}
