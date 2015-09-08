#pragma once

namespace kaiu {

namespace detail {

	function<void()> combine_finalizers(const function<void()> f1, const function<void()> f2);

}

namespace promise {

namespace monads {

/*
 * The operator that I want to use for "bind" is right-to-left associative, so I
 * need to store the operations then evaluate them myself in reversed order.
 *
 * I want to use this operator for its similarity to Haskell and for its low
 * precedence.  I initially considered the comma (which is left-to-right
 * associative and lowest precedence), but the default overload for it never
 * fails - which would create big problems regarding compile-time error
 * detection.
 */

/*
 * Callback packing operator (/), is evaluated before bind (>>=) operator
 * Combines into one bind_wrapper:
 *   next/handler
 *   next/handler/finalizer
 *   nullptr/handler
 *   nullptr/handler/finalizer
 */

template <typename Range, typename Domain>
const auto operator / (const Factory<Range, Domain> l, const Factory<Range, exception_ptr> r)
	{ return callback_pack<Range, Domain>(l, r); }

template <typename Range>
const auto operator / (const nullptr_t, const Factory<Range, exception_ptr> r)
	{ return callback_pack<Range, Range>(nullptr, r); }

template <typename Range, typename Domain>
const auto operator / (const callback_pack<Range, Domain>& l, const function<void()> r)
	{ return callback_pack<Range, Domain>(l.next, l.handler, detail::combine_finalizers(l.finalizer, r)); }

/*
 * Bind operators, each operand can be either of:
 *   bind_wrapper<Range, Domain>
 *   Factory<Range, Domain>
 */

template <typename Range, typename Middle, typename Domain>
const auto operator >>= (const callback_pack<Middle, Domain>& l, const callback_pack<Range, Middle>& r)
	{ return l.bind(r); }

template <typename Range, typename Middle, typename Domain>
const auto operator >>= (const Factory<Middle, Domain> l, const callback_pack<Range, Middle>& r)
	{ return callback_pack<Middle, Domain>(l).bind(r); }

template <typename Range, typename Middle, typename Domain>
const auto operator >>= (const callback_pack<Middle, Domain>& l, const Factory<Range, Middle> r)
	{ return l.bind(callback_pack<Range, Middle>(r)); }

template <typename Range, typename Middle, typename Domain>
const auto operator >>= (const Factory<Middle, Domain> l, const Factory<Range, Middle> r)
	{ return callback_pack<Range, Middle>(l).bind(callback_pack<Middle, Domain>(l)); }

}

}

}
