#pragma once
#include <functional>
#include <exception>
#include <cstddef>
#include <mutex>
#include <memory>
#include <type_traits>
#include <vector>
#include <tuple>
#include "self_managing.h"

#if defined(DEBUG)
#define SAFE_PROMISES
#endif

#include "promise/fwd.h"
#include "promise/traits.h"
#include "promise/factories.h"
#include "promise/combiners.h"
#include "promise/callback_pack.h"
#include "promise/monad.h"
#include "promise/state_base.h"
#include "promise/state.h"

namespace kaiu {

using namespace std;

/***
 * Promise class
 *
 * The default (no-parameter) constructor for Result type and for NextResult
 * type must not throw - promise chains guarantees will break.
 */

template <typename Result>
class Promise : public PromiseLike {
public:
	using DResult = typename remove_cvr<Result>::type;
	using result_type = DResult;
	/* Promise */
	Promise();
	/* Copy/move/cast constructors */
	Promise(const Promise<DResult>&);
	Promise(const Promise<DResult&>&);
	Promise(const Promise<DResult&&>&);
	Promise(Promise<Result>&&) = default;
	/* Assignment */
	Promise<Result>& operator =(Promise<Result>&&) = default;
	Promise<Result>& operator =(const Promise<Result>&) = default;
	/* Access promise state (then/except/finally/resolve/reject) */
	PromiseState<DResult> *operator ->() const;
private:
	friend class Promise<DResult>;
	friend class Promise<DResult&>;
	friend class Promise<DResult&&>;
	static_assert(!is_void<DResult>::value, "Void promises are no longer supported");
	static_assert(!is_same<DResult, exception_ptr>::value, "Promise result type cannot be exception_ptr");
	static_assert(!is_promise<DResult>::value, "Promise<Promise<T>> is invalid, use Promise<T>/forward_to instead");
	Promise(shared_ptr<PromiseState<DResult>> const promise);
	shared_ptr<PromiseState<DResult>> promise;
};

static_assert(is_promise<Promise<int>>::value, "Promise traits test #1 failed");
static_assert(!is_promise<int>::value, "Promise traits test #2 failed");
static_assert(result_of_promise_is<Promise<int>, int>::value, "Promise traits test #3 failed");
static_assert(!result_of_promise_is<Promise<int>, long>::value, "Promise traits test #4 failed");
static_assert(!result_of_promise_is<int, int>::value, "Promise traits test #5 failed");
static_assert(result_of_not_promise_is<int, int>::value, "Promise traits test #6 failed");
static_assert(!result_of_not_promise_is<int, long>::value, "Promise traits test #7 failed");
static_assert(!result_of_not_promise_is<Promise<int>, int>::value, "Promise traits test #8 failed");

}

#ifndef promise_tcc
#include "promise.tcc"
#endif
