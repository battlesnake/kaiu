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
	remove_lz();
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
	return !isZero();
}

bool decimal::isZero() const
{
	return length() == 1 && operator [](0) == 0;
}

bool decimal::isUnity() const
{
	return length() == 1 && operator [](0) == 1;
}

auto decimal::operator [](const size_t index) -> digit&
{
	return digits[index];
}

auto decimal::operator [](const size_t index) const -> digit
{
	return index < digits.size() ? digits[index] : 0;
}

size_t decimal::length() const
{
	return digits.size();
}

decimal decimal::operator +=(const decimal& b)
{
	if (isZero()) {
		*this = b;
		return *this;
	} else if (b.isZero()) {
		return *this;
	}
	decimal& a = *this;
	const size_t w = max(a.length(), b.length());
	digits.resize(w + 1);
	bool carry = false;
	for (size_t i = 0; i < w; i++) {
		digit d = a[i] + b[i] + (carry ? 1 : 0);
		carry = d >= 10;
		a[i] = carry ? d - 10 : d;
	}
	if (carry) {
		a[w] = 1;
	} else {
		digits.resize(w);
	}
	return *this;
}

decimal decimal::operator +(const decimal& b) const
{
	return decimal(*this) += b;
}

decimal decimal::operator ++()
{
	return operator +=(1);
}

decimal decimal::operator -=(const decimal& b)
{
	if (b.isZero()) {
		return *this;
	}
	decimal& a = *this;
	const size_t w = a.length();
	if (b.length() > w) {
		throw underflow_error("Negative values not allowed");
	}
	bool borrow = false;
	for (size_t i = 0; i < w; i++) {
		digit d = a[i] - b[i] - (borrow ? 1 : 0);
		borrow = d < 0;
		a[i] = borrow ? 10 + d : d;
	}
	if (borrow) {
		throw underflow_error("Negative values not allowed");
	}
	remove_lz();
	return *this;
}

decimal decimal::operator -(const decimal& b) const
{
	return decimal(*this) -= b;
}

decimal decimal::operator --()
{
	return operator -=(1);
}

decimal decimal::operator *(const decimal& value) const
{
	const decimal& a = length() <= value.length() ? *this : value;
	const decimal& b = length() > value.length() ? *this : value;
	if (a.isZero()) {
		return decimal(0);
	} else if (a.isUnity()) {
		return b;
	}
	decimal c;
	const auto as = a.length();
	const auto bs = b.length();
	c.digits.resize(as + bs);
	for (size_t i = 0; i < as; i++) {
		const digit ad = a[i];
		if (ad == 0) {
			continue;
		}
		decimal tmp;
		tmp.digits.resize(bs + i + 1);
		digit carry = 0;
		for (size_t j = 0; j < bs; j++) {
			digit d = b[j] * ad + carry;
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

decimal decimal::operator !() const {
	if (isZero() || isUnity()) {
		return decimal(1);
	}
	const decimal& x = *this;
	decimal r(x);
	for (decimal i(x - 1); !i.isUnity(); --i) {
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

bool decimal::operator ==(const decimal& b) const
{
	return digits == b.digits;
}

bool decimal::operator !=(const decimal& b) const
{
	return !operator ==(b);
}

char decimal::relation_to(const decimal& b) const
{
	const decimal& a = *this;
	const auto as = a.length();
	const auto bs = b.length();
	if (as > bs) {
		return +1;
	} else if (bs > as) {
		return -1;
	}
	for (auto i = as; i > 0; i--) {
		digit d = a[i - 1] - b[i - 1];
		if (d > 0) {
			return +1;
		} else if (d < 0) {
			return -1;
		}
	}
	return 0;
}

bool decimal::operator >(const decimal& b) const
{
	return relation_to(b) > 0;
}

bool decimal::operator >=(const decimal& b) const
{
	return relation_to(b) >= 0;
}

bool decimal::operator <(const decimal& b) const
{
	return relation_to(b) < 0;
}

bool decimal::operator <=(const decimal& b) const
{
	return relation_to(b) <= 0;
}

}

#ifdef test_decimal
#include <chrono>
#include "assertion.h"

using namespace std;
using namespace std::chrono;
using namespace mark;

Assertions assert({
	{ nullptr, "Construction & output" },
	{ "0is", "Initialize from int, convert 0 to string" },
	{ "1is", "Initialize from int, convert 1 to string" },
	{ "32is", "Initialize from int, convert 32 to string" },
	{ "5678is", "Initialize from int, convert 5678 to string" },
	{ "0si", "Initialize from string, convert 0 to int" },
	{ "1si", "Initialize from string, convert 1 to int" },
	{ "32si", "Initialize from string, convert 32 to int" },
	{ "5678si", "Initialize from string, convert 5678 to int" },
	{ nullptr, "Basic operations" },
	{ "2=2", "Equality" },
	{ "2≠3", "Inequality" },
	{ "2+2", "Addition" },
	{ "99+2", "Addition with carry" },
	{ "5-2", "Subtraction" },
	{ "102-5", "Subtraction with borrow" },
	{ "1234*5678", "Multiplication" },
	{ nullptr, "Factorial" },
	{ "0!", "Zero" },
	{ "1!", "One" },
	{ "6!", "Small" },
	{ "5000!", "Large" },
});

int main(int argc, char *argv[])
{
	assert.expect(string(decimal(0)), "0", "0is");
	assert.expect(string(decimal(1)), "1", "1is");
	assert.expect(string(decimal(32)), "32", "32is");
	assert.expect(string(decimal(5678)), "5,678", "5678is");
	assert.expect(int(decimal("0")), 0, "0si");
	assert.expect(int(decimal("1")), 1, "1si");
	assert.expect(int(decimal("0032")), 32, "32si");
	assert.expect(int(decimal("0,005,678")), 5678, "5678si");
	assert.expect(decimal(2), decimal(2), "2=2");
	assert.expect(decimal(2) != decimal(3), true, "2≠3");
	assert.expect(decimal(2) + decimal(2), 4, "2+2");
	assert.expect(decimal(99) + decimal(2), 101, "99+2");
	assert.expect(decimal(5) - decimal(2), 3, "5-2");
	assert.expect(decimal(102) - decimal(5), 97, "102-5");
	assert.expect(decimal(1234) * decimal(5678), 7006652, "1234*5678");
	assert.expect(!decimal(0), 1, "0!");
	assert.expect(!decimal(1), 1, "1!");
	assert.expect(!decimal(6), 720, "6!");
	const auto start = system_clock::now();
	const decimal fac1k = !decimal(5000);
	const auto duration = duration_cast<milliseconds>(system_clock::now() - start);
	assert.expect(fac1k.length() == 16326 && fac1k[0] == 0 && fac1k[fac1k.length() - 1] == 4, true, "5000!", to_string(duration.count()) + string("ms"));
}
#endif
