#include "decimal.h"

namespace mark {

decimal::decimal() :
	digits{0} 
{
}

decimal::decimal(const string& val)
{
	digits.reserve(val.size());
	for (auto it = val.crbegin(); it != val.crend(); ++it) {
		char c = *it;
		if (c == ',') {
			continue;
		}
		if (c >= '0' && c <= '9') {
			digits.push_back((digit) (c - '0'));
		} else {
			throw invalid_argument("Not a digit: " + string{c});
		}
	}
}

decimal::operator string() const
{
	const size_t w = digits.size();
	size_t copied = 0;
	string s;
	s.reserve(w + (w ? (w - 1) / 3 : 0));
	for (auto const digit : digits) {
		if (copied > 0 && copied % 3 == 0) {
			s.push_back(',');
		}
		s.push_back((char) (digit + '0'));
		copied++;
	}
	/* Reverse */
	string t;
	t.reserve(s.size());
	for (auto it = s.crbegin(); it != s.crend(); ++it) {
		t.push_back(*it);
	}
	return t;
}

decimal::operator bool() const
{
	for (const auto digit: digits) {
		if (digit) {
			return true;
		}
	}
	return false;
}

auto decimal::operator [](const int index) -> digit&
{
	return digits[index];
}

auto decimal::operator [](const int index) const -> digit
{
	return index < digits.size() ? digits[index] : 0;
}

size_t decimal::length() const
{
	return digits.size();
}

decimal decimal::operator +=(const decimal& b)
{
	decimal& a = *this;
	const size_t w = max(a.length(), b.length());
	digits.resize(w + 1);
	digit carry = 0;
	for (size_t i = 0; i < w; i++) {
		digit d = a[i] + b[i] + carry;
		carry = d / 10;
		a[i] = d - (carry * 10);
	}
	if (carry) {
		a[w] = carry;
	} else {
		digits.resize(w);
	}
	return *this;
}

decimal decimal::operator +(const decimal& b) const
{
	decimal c(*this);
	c += b;
	return c;
}

decimal decimal::operator *(const decimal& value) const
{
	const decimal& a = length() < value.length() ? *this : value;
	const decimal& b = length() >= value.length() ? *this : value;
	decimal c;
	const auto as = a.length();
	const auto bs = b.length();
	c.digits.resize(as + bs);
	for (size_t i = 0; i < as; i++) {
		decimal tmp;
		tmp.digits.resize(bs + i + 1);
		digit carry = 0;
		for (size_t j = 0; j < bs; j++) {
			digit d = b[j] * a[i] + carry;
			carry = d / 10;
			tmp[j + i] = d - (carry * 10);
		}
		if (carry) {
			tmp[bs + i] = carry;
		} else {
			tmp.digits.resize(bs + i);
		}
		c += tmp;
	}
	c.remove_lz();
	return c;
}

decimal decimal::operator *=(const decimal& b)
{
	decimal a(*this);
	*this = a * b;
	return *this;
}

decimal decimal::operator --()
{
	bool borrow;
	size_t i;
	const auto w = length();
	for (i = 0, borrow = true; i < w && borrow; i++) {
		if (digits[i] == 0) {
			digits[i] = 9;
		} else {
			digits[i]--;
			borrow = false;
		}
	}
	if (borrow) {
		throw underflow_error("Negative values not allowed");
	}
	remove_lz();
	return *this;
}

decimal decimal::operator !() const {
	const decimal& x = *this;
	decimal r(1);
	for (decimal i(x); i; --i) {
		r *= i;
	}
	return r;
}

void decimal::remove_lz()
{
	const auto w = length();
	for (auto i = w; i > 0; i--) {
		if (digits[i - 1] != 0) {
			digits.resize(i);
			return;
		}
	}
	digits.resize(1);
}

}
