#include <string>
#include <chrono>
#include "decimal.h"
#include "assertion.h"

using namespace std;
using namespace std::chrono;
using namespace kaiu;

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
try {
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
	return assert.print(argc, argv);
} catch (...) {
	assert.print_error();
}
