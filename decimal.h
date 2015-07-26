#pragma once
#include <vector>
#include <string>
#include <type_traits>
#include <limits>

/*
 * Not intended for production use, just something to provide load for testing
 * other stuff - hence base being fixed at 10 and there being no sign bit.
 */

namespace mark {

using namespace std;

class decimal {
public:
	using digit = char;
	decimal();
	template <typename T, typename = typename enable_if<numeric_limits<T>::is_integer>::type>
	decimal(T val);
	decimal(const string& val);
	explicit operator string() const;
	explicit operator bool() const;
	template <typename T, typename = typename enable_if<numeric_limits<T>::is_integer>::type>
	explicit operator T() const;
	digit& operator [](const size_t index);
	digit operator [](const size_t index) const;
	size_t length() const;
	decimal operator +=(const decimal& b);
	decimal operator +(const decimal& b) const;
	decimal operator ++();
	decimal operator -=(const decimal& b);
	decimal operator -(const decimal& b) const;
	decimal operator --();
	decimal operator *(const decimal& b) const;
	decimal operator *=(const decimal& b);
	/* Haha ***k you `if (!value) { ... }` */
	decimal operator !() const;
	bool operator ==(const decimal& b) const;
	bool operator !=(const decimal& b) const;
	bool operator >(const decimal& b) const;
	bool operator >=(const decimal& b) const;
	bool operator <(const decimal& b) const;
	bool operator <=(const decimal& b) const;
	template <typename T, typename = typename enable_if<numeric_limits<T>::is_integer>::type>
	bool operator ==(const T& b) const;
	static decimal parallel_multiply(const decimal&, const decimal&);
private:
	vector<digit> digits;
	void remove_lz();
	char relation_to(const decimal& b) const;
	bool isZero() const;
	bool isUnity() const;
};

}

#ifndef decimal_tcc
#include "decimal.tcc"
#endif
