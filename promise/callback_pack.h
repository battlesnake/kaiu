#pragma once

namespace kaiu {

namespace promise {

/*
 * Can be used to pack callbacks for a promise, for use in monad syntax
 */

template <typename Range, typename Domain>
class callback_pack {
public:
	using range_type = Range;
	using domain_type = Domain;
	static constexpr bool is_terminal = is_void<Range>::value;
	using Next = typename conditional<
		is_terminal,
		function<void(Domain)>,
		function<Promise<Range>(Domain)>>::type;
	using Handler = typename conditional<
		is_terminal,
		function<void(exception_ptr)>,
		function<Promise<Range>(exception_ptr)>>::type;
	using Finalizer = function<void()>;
	/* Pack callbacks */
	explicit callback_pack(const Next next, const Handler handler = nullptr, const Finalizer finalizer = nullptr);
	/* Bind operator, for chaining callback packs */
	template <typename NextRange>
	auto bind(const callback_pack<NextRange, Range> after) const;
	/* Bind callbacks to promise */
	Promise<Range> operator () (const Promise<Domain> d) const;
	Promise<Range> operator () (Domain d) const;
	Promise<Range> reject(exception_ptr error) const;
	const Next next;
	const Handler handler;
	const Finalizer finalizer;
private:
	Promise<Range> call (const Promise<Domain> d) const;
};

}

}
