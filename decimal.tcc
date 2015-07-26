#define decimal_tcc
#include <stdexcept>
#include "decimal.h"

namespace kaiu {

using namespace std;

template <typename T, typename>
decimal::decimal(T val)
{
	if (val < 0) {
		throw invalid_argument("Negative value not allowed");
	}
	digits.reserve(int(numeric_limits<T>::digits10 + 1));
	if (val) {
		while (val) {
			digits.push_back(val % 10);
			val /= 10;
		}
	} else {
		digits.push_back(0);
	}
}

template <typename T, typename>
decimal::operator T() const
{
	const T max = numeric_limits<T>::max();
	T r = 0;
	for (auto digit = digits.crbegin(); digit != digits.crend(); ++digit) {
		if ((max - *digit) / 10 < r) {
			throw overflow_error("Overflow in conversion to fixed-width integer");
		}
		r = (r * 10) + *digit;
	}
	return r;
}

template <typename T, typename>
bool decimal::operator ==(const T& b) const
{
	return operator ==(decimal(b));
}

}
