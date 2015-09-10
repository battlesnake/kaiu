#pragma once

namespace kaiu {

namespace promise {

namespace monads {

/* Bind stream factories to create promise factories */

template <typename Result, typename State, typename Datum, typename... Args>
auto operator >= (StreamFactory<Result, Datum, Args...> l,
	StatefulConsumer<State, Result> r)
{
	promise::Factory<pair<State, Result>, Args...> factory =
		[l, r] (Args&&... args) -> Promise<pair<State, Result>> {
			return l(forward<Args>(args)...)
				->template stream<State>(r);
		};
	return factory;
}

template <typename Result, typename Datum, typename Functor, typename... Args,
	typename = typename result_of<Functor(Datum)>::type>
auto operator >= (StreamFactory<Result, Datum, Args...> l, Functor r)
{
	promise::Factory<Result, Args...> factory =
		[l, r] (Args&&... args) -> Promise<Result> {
			return l(forward<Args>(args)...)
				->template stream<void>(r);
		};
	return factory;
}

}

}

}
