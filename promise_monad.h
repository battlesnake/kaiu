#pragma once
#include <functional>
#include "promise.h"

namespace kaiu {

namespace promise {

namespace monads {

using namespace std;

/*
 * The operators that I want to use are right-to-left associative, so I need to
 * store the operations then evaluate them myself in reversed order.
 */

namespace detail {

template <typename Range, typename Domain,
	typename = typename enable_if<!is_same<exception_ptr, Range>::value>::type,
	typename = typename enable_if<!is_promise<Range>::value>::type,
	typename = typename enable_if<!is_same<exception_ptr, Domain>::value>::type,
	typename = typename enable_if<!is_promise<Domain>::value>::type>
struct bind_wrapper {
	bind_wrapper() = delete;
	/* Combine (for / operator) */
	explicit bind_wrapper(const Factory<Range, Domain> next, const Factory<Range, exception_ptr> handler = nullptr, const function<void()> finalizer = nullptr) :
		next(next), handler(handler), finalizer(finalizer)
			{ }
	explicit bind_wrapper(const bind_wrapper<Range, Domain> th, const function<void()> finalizer) :
		bind_wrapper(th.next, th.handler, finalizer)
			{ if (th.finalizer) throw logic_error("Too many callbacks combined by '/' operator"); }
	/* Bind */
	template <typename PreDomain>
	bind_wrapper<Range, PreDomain> bind_to(bind_wrapper<Domain, PreDomain> l) const
	{
		return bind_wrapper<Range, Domain> {
			Factory<Range, PreDomain> {
				[l, next=next, handler=handler, finalizer=finalizer] (PreDomain d) {
					return l(move(d))->then(next, handler, finalizer);
				}
			}
		};
	}
	/* Execute */
	Promise<Range> call(const Promise<Domain> initial) const
		{ return initial->then(next, handler, finalizer); }
	Promise<Range> operator () (const Promise<Domain> initial) const
		{ return call(initial); }
	Promise<Range> operator () (Domain initial) const
		{ return call(promise::resolved<Domain>(move(initial))); }
	Promise<Range> operator () (exception_ptr initial) const
		{ return call(promise::rejected<Domain>(initial)); }
	/* Callbacks */
	const Factory<Range, Domain> next{nullptr};
	const Factory<Range, exception_ptr> handler{nullptr};
	const function<void()> finalizer{nullptr};
};

}

using detail::bind_wrapper;

/*
 * Callback packing operator (/), evaluated before bind (>>= |=) operators
 * Combines into one bind_wrapper:
 *   next/handler
 *   next/handler/finalizer
 *   nullptr/handler
 *   nullptr/handler/finalizer
 */

template <typename Range, typename Domain>
auto operator / (const Factory<Range, Domain> l, const Factory<Range, exception_ptr> r)
	{ return bind_wrapper<Range, Domain>(l, r); }

template <typename Range>
auto operator / (const nullptr_t, const Factory<Range, exception_ptr> r)
	{ return bind_wrapper<Range, Range>(nullptr, r); }

template <typename Range, typename Domain>
auto operator / (const bind_wrapper<Range, Domain> l, const function<void()> r)
	{ return bind_wrapper<Range, Domain>(l, r); }

/*
 * Bind operators, each operand can be either of:
 *   bind_wrapper<Range, Domain>
 *   Factory<Range, Domain>
 */

template <typename Range, typename Middle, typename Domain>
auto operator >>= (const bind_wrapper<Middle, Domain> l, const bind_wrapper<Range, Middle> r)
	{ return r.bind_to(l); }

template <typename Range, typename Middle, typename Domain>
auto operator >>= (const Factory<Middle, Domain> l, const bind_wrapper<Range, Middle> r)
	{ return r.bind_to(bind_wrapper<Middle, Domain>(l)); }

template <typename Range, typename Middle, typename Domain>
auto operator >>= (const bind_wrapper<Middle, Domain> l, const Factory<Range, Middle> r)
	{ return bind_wrapper<Range, Middle>(r).bind_to(l); }

template <typename Range, typename Middle, typename Domain>
auto operator >>= (const Factory<Middle, Domain> l, const Factory<Range, Middle> r)
	{ return bind_wrapper<Range, Middle>(r).bind_to(bind_wrapper<Middle, Domain>(l)); }

}

}

}
