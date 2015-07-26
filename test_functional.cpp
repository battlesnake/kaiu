#define enable_monads
#include <iostream>
#include <functional>
#include <math.h>
#include "assertion.h"
#include "task.h"
#include "functional.h"

using namespace std;
using namespace kaiu;

Assertions assert({
	{ nullptr, "Basic currying" },
	{ "BR", "R-value curried" },
	{ "BL", "L-value curried" },
	{ nullptr, "Cross-thread currying via apply" },
	{ "ACR", "R-value curried by value" },
	{ "ACL", "L-value curried by reference" },
	{ nullptr, "Cross-thread currying via operator()" },
	{ "AOCR", "R-value curried by value" },
	{ "AOCL", "L-value curried by reference" },
	{ nullptr, "Cross-thread currying via operator <<" },
	{ "SHCR", "R-value curried by value" },
	{ "SHCL", "L-value curried by value" },
	{ "SHCWL", "Reference-wrapped L-value curried by reference" },
	{ nullptr, "Modads" },
	{ "MONADS", "Synchronous monad chain (never use in production)" },
	{ "MONADA", "Asynchronous cross-thread monad chain" },
});

int sqr__(int x)
{
	return x * x;
}

/*
 *                      ╕╥╠▒╬▓▄
 *                     ¬≈╫██████Mù,▄▄            .;░╥úúë;▒▒▒▒╦╥    ²
 *                     ╩  ╙╫▒╨╢██╥/-╠█╩▒╩▒▒▒▒▓╗▄µ░ú▒▒╥╦▒▓▒▒▒▒▒▒╫▓▄
 *                         ╙╬ ╨╫██▒▒▒╫█═▓▒▒▒▓▓▓▓▓▒▒▒▒╫╫▒▒▒▒▒▒▒▒▒▒▓█
 *                          ▌  ▒╫█▓▒╫╫▒▒▒╫████████▓▓▒▒▒╫▒╫▒╬▓▓▒▒▓██
 *                          ╫  █▒▓▓▓▒▓██╫╬▓███████▓╣▓▓▓▓▓╫▓▓███████▌
 *                          ╚  ╫▓█▓▓█▓╫▒▓╬███████▓▓▓██╫▓████████████
 *    a²   +   b²   =       ║  .╬▓▓███▒▒╫╣▓███████████▓█████████████
 *                         ,╙  ⌂║█▒╬▓█Ü▒▒╬█▓██▓█████████████████████
 *                      "Θ╗—-j, ²▒▒▓█▒▓▓█▓▓████████████████████████▌
 *                        - ╔ `╖╬██  "▀╬▒╬▓████████████████████████
 *                        `ⁿ¥═≥▀^       `██████████████████████████
 *                                       ╙███▓███████████████▀█████
 *                                        ╙██████"███"▀████▌  ████▌
 *                                          ╙████ ███  █████  ████▌
 */
int hippopotenuse(int x, int y)
{
	return sqrt(sqr__(x) + sqr__(y));
}

void *logOutput(const string line, const int value)
{
	cout << line << ": " << value << endl;
	return nullptr;
}

void *test__(int a, const string& code, int b)
{
	assert.expect(a, b, code);
	return nullptr;
}

ParallelEventLoop loop{ {
	{ EventLoopPool::reactor, 1 },
	{ EventLoopPool::calculation, 1 }
} };

const auto hippo = Curry<int, 2>(hippopotenuse);
const auto sqr = Curry<int, 1>(sqr__);
const auto test = Curry<void*, 3>(test__);

const auto Hippo = promise::task(promise::factory(hippopotenuse),
	EventLoopPool::calculation,
	EventLoopPool::reactor) << ref(loop);

const auto Sqr = promise::task(promise::factory(sqr__),
	EventLoopPool::calculation,
	EventLoopPool::reactor) << ref(loop);

const auto Test = promise::task(promise::factory(test__),
	EventLoopPool::reactor,
	EventLoopPool::reactor) << ref(loop);

const auto LogOutput = promise::task(promise::factory(logOutput),
	EventLoopPool::reactor) << ref(loop);

void basic_tests()
{
	const auto hippo = Curry<int, 2>(hippopotenuse);
	assert.expect(hippo.apply(3, 4)(), 5, "BR");
	const int x = 3, y = 4;
	assert.expect(hippo.apply(x, y)(), 5, "BL");
}

void cross_thread_currying()
{
	/* AC */
	{
		Hippo.apply(3, 4).invoke()
			->then(
				[] (int z) {
					assert.expect(z, 5, "ACR");
				},
				[] (auto& e) {
					assert.fail("ACR");
				});
		int x = 5, y = 12;
		const auto fxy = Hippo.apply(x, y);
		x = 3, y = 4;
		fxy.invoke()
			->then(
				[] (int z) {
					assert.expect(z, 5, "ACL");
				},
				[] (auto& e) {
					assert.fail("ACL");
				});
	}
	/* AOC */
	{
		Hippo(3, 4)->then(
			[] (int z) {
				assert.expect(z, 5, "AOCR");
			},
			[] (auto& e) {
				assert.fail("AOCR");
			});
		int x = 5, y = 4;
		const auto fx = Hippo.apply(x);
		x = 3;
		fx(y)->then(
			[] (int z) {
				assert.expect(z, 5, "AOCL");
			},
			[] (auto& e) {
				assert.fail("AOCL");
			});
	}
	/* SHC */
	{
		(Hippo << 3 << 4).invoke()->then(
			[] (int z) {
				assert.expect(z, 5, "SHCR");
			},
			[] (auto& e) {
				assert.fail("SHCR");
			});
		int x = 5, y = 12;
		const auto fxy = Hippo << x << y;
		x = 3, y = 4;
		fxy()->then(
			[] (int z) {
				assert.expect(z, 13, "SHCL");
			},
			[] (auto& e) {
				assert.fail("SHCL");
			});
	}
	{
		int x = 5, y = 12;
		const auto fxy = Hippo << ref(x) << cref(y);
		x = 35;
		fxy()->then(
			[] (int z) {
				assert.expect(z, 37, "SHCWL");
			},
			[] (auto& e) {
				assert.fail("SHCWL");
			});
	}
	loop.join();
}

void monad_tests()
{
	/*** Do not use synchronous monads, they're here for fun only ***/
	/* Synchronous monad, operator in functional.h */
	4, sqr, hippo << 63, test << 65 << "MONADS";
	/* Asynchronous monad, operator in task.h */
	promise::resolved(3) | Sqr | Hippo << 40 | Test << 41 << "MONADA";
}

int main(int argc, char **argv)
{
	auto printer = assert.printer();

	basic_tests();
	cross_thread_currying();
	monad_tests();

//	/*
//	 * Play with Pythagorean triples:
//	 * 3:4:5
//	 * 5:12:13
//	 * 8:13:15
//	 */
//	auto h3 = Hippo.apply(3);
//
//	h3(4)
//		->then(Hippo << 12)
//		->then(Hippo << 8)
//		->then(LogOutput << "Should be 15");

	loop.join();
	return assert.print(argc, argv);
}
