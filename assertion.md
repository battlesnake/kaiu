Assertion
=========

Basic unit-testing tool.

Build list of assertions identified by a code and a human-friendly description,
(separated into groups):

	Assertions assert({
		{ nullptr, "nullptr code means this is a group title" },
		{ "TEST", "non-nullptr code means this is an assertion" },
		{ "LEMON", "codes can be anything you want so long as they're unique" },
		{ nullptr, "Another group" },
		{ "2+2", "Does maths work?" }
	};

	int main(int argc, char **argv)
	{
		/*
		 * Uses RAII to ensure that results are printed even if an uncaught
		 * exception occurs
		 */
		auto printer = assert.printer();

		assert.expect(2 + 2, 4, "2+2", "Yes");

		assert.fail("TEST");

		/* --test-silent-if-perfect causes short output if all tests passed */
		return assert.print(argc, argv);
	}

All tests must either pass or be skipped in order for the return value of
`assert.print` to be zero.  Any tests which are not explicitly passed, skipped,
or failed will be reported as "missed".

Constructor:

	Assertions assert(unordered_map<pair<char*, char*>>);

Store results:

	assert.pass(code [, note]);
	assert.fail(code [, note]);
	assert.skip(code [, note]);
	assert.set(code, state [, note]);
	assert.expect(a, b, code [, note]);

The `expect` method marks the test as passed if `a == b`, and marks it as failed
otherwise.  The optional `note` will be displayed next to the test description
in the output from `assert.print`.

	assert.print(always);
	assert.print(int argc, char *argv[]);

`always` causes full output to be printed even when all tests are passed.
The other form will result in `always` being set to true if the parameter
`--test-silent-if-perfect` is not specified in `argv`.

The destructor `Assertions::~Assertions` will call `print` if it has not already
been called.

	auto printer = assert.printer();

When the `printer` is RAII-destructed, it will call `assert.print` if `print`
has not already been called.
