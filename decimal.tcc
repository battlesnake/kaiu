#define decimal_tcc
#include <stdexcept>
#include "decimal.h"

namespace mark {

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

}
