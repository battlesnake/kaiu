#include <thread>
#include <future>
#include <mutex>
#include <algorithm>
#include "decimal.h"

namespace kaiu {

using namespace std;

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

decimal decimal::parallel_multiply(const decimal& l, const decimal& r)
{
	const decimal& a = l.length() <= r.length() ? l : r;
	const decimal& b = l.length() > r.length() ? l : r;
	if (a.isZero()) {
		return decimal(0);
	} else if (a.isUnity()) {
		return b;
	}
	const auto as = a.length();
	const auto bs = b.length();
	const size_t cores = std::thread::hardware_concurrency();
	const size_t workers = min<size_t>(cores, (as / 1000));
	if (workers == 0) {
		return l * r;
	}
	decimal result;
	result.digits.resize(as + bs);
	mutex mx;
	function<int(size_t, size_t)> partial_product = [&a, &b, as, bs, &mx, &result] (size_t begin, size_t end) {
		decimal c;
		c.digits.resize(as + bs);
		decimal tmp;
		tmp.digits.reserve(bs + end + 1);
		for (size_t i = begin; i < end; i++) {
			const digit ad = a[i];
			if (ad == 0) {
				continue;
			}
			fill(tmp.digits.begin(), tmp.digits.end(), 0);
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
		lock_guard<mutex> lock(mx);
		result += c;
		return 0;
	};
	vector<future<int>> futures;
	futures.reserve(workers);
	for (size_t worker = 0; worker < workers; worker++) {
		const auto begin = (as * worker) / workers;
		const auto end = (as * (worker + 1)) / workers;
		futures.emplace_back(async(std::launch::async, partial_product, begin, end));
	}
	for (size_t worker = 0; worker < workers; worker++) {
		auto& future = futures[worker];
		future.get();
	}
	result.remove_lz();
	return result;
}

}

