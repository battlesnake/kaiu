Tuple iteration
===============

Functional patterns for tuples.

For-each iteration over tuple:

	tuple_each(tuple, functor)

For-each iteration over tuple and indices:

	tuple_each_with_index(tuple, functor)

Transform one tuple into another one (of possibly different data types):

	tuple_map(tuple, functor)

Trivial but not implemented yet:

	tuple_map_with_index(tuple, functor)

The `_with_index` flavours take a functor with the following operator:

	template <typename Type, size_t index>
	Result operator () (Type& value)

Where `Result` is ignored for `each` and is the mapped value for `map`.

The not-`_with_index` flavours do not have the `index` template parameter.

The (promise)[https://github.com/battlesnake/kaiu/blob/master/promise.md]
combiners use the `tuple_each_with_index` function.
