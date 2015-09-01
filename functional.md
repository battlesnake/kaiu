Functional
==========

Lambda-calculus goodies:

Curry
-----

	float quadratic(float a, float b, float c, float x)
	{
		return (a * x + b) * x + c;
	}

	/* Wrap "quadratic" function in currying wrapper */
	auto quad = Curry<float, 4>(quadratic);

	/* Bind (a, b, c) to values (1, 1, 1) */
	auto golden = quad.apply(1, 1, -1);

	assert(fabs(golden(0.61803398875)) < 1e-4);

`Curry<Result, Arity>(func, args...)` takes a function `func` with arity `Arity`
and return type `Result`, and wraps the function in a currying object.  Any
extra arguments `args...` to `Curry` are bound to the function.

The wrapper is immutable (i.e. its own methods will not modify it).

For example, given either of the following:

	auto hyp = Curry<double, 2>(hypot, 3);
	auto hyp = Curry<double, 2>(hypot).apply(3);

The following all give the same result:

	hypot(3, 4)
	hyp(4)
	hyp.apply(4).invoke()
	hyp.apply(4)()
	(hyp << 4)()

Apply
-----

The function-style ways to call apply will capture by reference whenever
possible, for example:

Given:

	double x = 3;

Then either of these function-style function applications:

	auto hyp = Curry<double, 2>(hypot, x);
	auto hyp = Curry<double, 2>(hypot).apply(x);

The following evaluate to `hypot(3, 4)`

	hyp(4);
	hyp.apply(4).invoke()

Then we change `x`

	x = 5;

The following evaluate to `hypot(5, 4)`

	hyp(4);
	hyp.apply(4).invoke()

The `<<`-style way to call apply will capture only by value, unless the operand
is in a reference wrapper:

Given:

	double x = 3;

Then the `<<`-style function application:

	auto hyp = Curry<double, 2>(hypot) << x;

The following evaluate to `hypot(3, 4)`

	hyp(4);
	hyp.apply(4).invoke()

Then we change `x`

	x = 5;

The following still evaluate to `hypot(3, 4)`, as `x` was captured by value:

	hyp(4);
	hyp.apply(4).invoke()

If we had wanted to capture `x` by reference, using the `<<`-style, we would
wrap `x` in a reference wrapper:

	auto hyp = Curry<double, 2>(hypot) << std::ref(x);

Invoke
------

To invoke a curried function, use the `()` operator or the `invoke` method.

The function operator can pass extra arguments to the function, which is usually
what you would want to do when currying:

	curried(args...)

The `invoke` method does not pass extra arguments:

	curried.invoke()

The function operator is basically a shorthand for `apply` followed by `invoke`.

Lazy-evaluation
---------------

By binding all arguments to a function with `apply`, we can use curried
functions as a form of lazy evaluation:

	// Does not invoke the (possibly expensive) function
	auto flow_gen = solve_navier_stokes_using_fem.apply(args...);

	// Run a simulation which may not require the result of the expensive
	// function above (e.g. if the simulation fails before needing that data).
	simulate_machine(..., flow_gen, ...);

	// In simulate_machine, to evaluate the flow when required:
	auto flow = flow_gen();

In this example, the `simulate_machine` function exeutes
`solve_navier_stokes_using_fem` on demand, but does not need to know all of
the simulation parameters and configuration - it can be given several curried
lazy-evaluated functions which already contain their necessary parameters, and
which can then be evaluated on demand.  In this style we avoid god-objects and
insanely long parameter lists, by composing lazy-evaluated functions out of
more lazy-evaluated functions, each of which are bound to only the parameters
which they require.

bind
----

If `ENABLE_FUNCTIONAL_BIND` is #define'd then a horribly comma operator overload
will be defined.

Given:

	double square(double x)
	{
		return x * x;
	}

	double addition(double a, double b)
	{
		return a + b;
	}

	void *printer(string s, double x)
	{
		cout << s << ": " << x << endl;
		return nullptr;
	}

	auto hyp = Curried<double, 2>(hypot);
	auto root = Curried<double, 1>(sqrt);
	auto sqr = Curried<double, 1>(sqr);
	auto add = Curried<double, 2>(add);
	auto print = Curried<void*, 2>(printer);

We can then do:

	3, hyp << 4, sqr, add << 9, root, print << "Should be 6";

Which is equivalent to:

	print("Should be 6", sqrt(addition(9, square(hypot(4, 3)))));

Although more readable for one-liners, whether it is actually a good idea to use
this operator is a decision° that I leave to you.

° of course it isn't a good idea - we're overloading the comma FFS!

The bind operator requires that the right-hand-side operand is a curried
function with remaining arity of 1 (i.e. it requires exactly one more argument
before it can be evaluated).
