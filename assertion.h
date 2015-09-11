#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <mutex>
#include <chrono>
#include <tuple>

namespace kaiu {


class Assertions {
public:
	enum result { unknown, skipped, passed, failed };
	Assertions() = delete;
	Assertions(const Assertions&) = delete;
	Assertions(const std::vector<std::pair<const char *, const char *>>& strings);
	~Assertions();
	void set(const std::string& code, const result state, const std::string& note = "");
	void pass(const std::string& code, const std::string& note = "");
	void fail(const std::string& code, const std::string& note = "");
	void skip(const std::string& code, const std::string& note = "");
	void try_pass(const std::string& code, const std::string& note = "");
	template <typename T, typename U>
	void expect(const T& t, const U& u, const std::string& assertion, const std::string& note = "");
	int print(bool always);
	int print(const int argc, char const * const argv[]);
	void print_error();
private:
	using ensure_locked = const std::lock_guard<std::mutex>&;
	std::chrono::time_point<std::chrono::steady_clock> start_time{std::chrono::steady_clock::now()};
	std::mutex mx;
	bool printed;
	const std::vector<std::pair<const char *, const char *>> strings;
	std::unordered_map<std::string, std::pair<result, std::string>> list;
	int _print(ensure_locked, bool always);
	void _set(ensure_locked, const std::string& code, const result state, const std::string& note);
	std::pair<result, std::string>& _get(ensure_locked, const std::string& code);
};

template <typename Actual, typename Expect>
void Assertions::expect(const Actual& actual, const Expect& expect, const std::string& assertion, const std::string& note)
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
