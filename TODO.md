Tasks
=====

Use std::bind to enable support for currying in task factories:

	int quadratic_func(int x, int a, int b, int c) {
		...
	}

	auto quadratic = task::make_factory(..., quadratic_func, ..., ...);

Since `quadratic_func` takes multiple parameters, we cannot invoke it directly:

	/*
	 * Invalid: Promise result cannot be variadic (although I once considered
	 * it)
	 */
	promise::resolved(x, a, b, c)
		->then_p<int>(quadratic)
		->...

The current solution is to create a wrapper function/template that expands a
tuple of parameters and calls the original function, or which automatically
fills in some parameters with defaults - and then to use this wrapper to
construct our task.  This is not DRY, not versatile, and I hate it.

	/* Bad - other parameters are fixed and we have WET */
	int quadratic_wrap(int x) {
		const int a = ..., b = ..., c = ...;
		return quadratic_func(x, a, b, c);
	}

	/* Still not great - explicitly defined WET function factories */
	int quadratic_curry(int a, int b, int c) {
		return [a, b, c] (int x) {
			return quadratic_func(x, a, b, c);
		};
	}

With currying, one could instead do:

	int quadratic_func(x, a, b, c) {
		...
	}

	auto quadratic = task::make_factory(..., quadratic_func, ..., ...);

	/* Currying via std::bind and a function operator on the factory */
	promise::resolved(x)
		->then_p<int>(quadratic(task::input, a, b, c))
		->...

	/* And also (particularly useful for loggers/formatters) */
	auto calculate = bind(quadratic, task::input, a, b, c)
	auto calculate2 = bind(quadratic, task::input, p, q, r)
	promise::resolved(x)
		->then_p<int>(calculate)
		->...
		->then_p<int>(calculate2)
		->...

The item `mark::task::input` is just an alias for `std::placeholders::_1`.

Note that `quadratic` is now no longer a promise factory - the layer of currying
essentially makes it a promise factory factory now.  So to launch a task with
the default parameter layout (i.e. one parameter, result of previous promise),
we can use an empty parameter list instead of currying:

	int single_parameter_function(int x) {
		...
	}

	auto singleParameterFunctionWrappedAsTask = task::make_factory(...);

	promise::resolved(x)
		->then_p<int>(singleParameterFunctionWrappedAsTask())
		->...

An empty parameter list `()` should be functionally identical to `(task::input)`
in this case.
