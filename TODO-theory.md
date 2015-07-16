Function:

	f(args...) => result

Promise factory:

	F(args...) => P<result>

	promise::make_factory(f) => F

Bind:

	N(x) => P<result>

	promise::make_next(F) => N


