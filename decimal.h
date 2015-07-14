#pragma once
#include <vector>
#include <string>
#include <type_traits>
#include <limits>

namespace mark {

using namespace std;

class decimal {
public:
	using digit = unsigned char;
	decimal();
	template <typename T, typename = typename enable_if<numeric_limits<T>::is_integer>::type>
	decimal(T val);
	decimal(const string& val);
	operator string() const;
	operator bool() const;
	digit& operator [](const int index);
	digit operator [](const int index) const;
	size_t length() const;
	decimal operator +=(const decimal& b);
	decimal operator +(const decimal& b) const;
	decimal operator *(const decimal& b) const;
	decimal operator *=(const decimal& b);
	decimal operator --();
	decimal operator !() const;
private:
	vector<digit> digits{};
	void remove_lz();
};

}

#ifndef decimal_tcc
#include "decimal.tcc"
#endif
