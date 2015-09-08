#pragma once

namespace kaiu {

/* Non-template base class for PromiseState */
class PromiseStateBase;

/* For internal use, represents state of a promise */
template <typename Result> class PromiseState;

/*
 * A base class for anything that behaves like a promise.
 * is_promise_like<T> returns true if T inherits from PromiseLike
 *
 * Result type of a promise cannot derive from (or be) PromiseLike.
 *
 * PromiseLike should support ->resolve ->reject ->forward_to:
 *   void resolve(T)
 *   void reject(exception_ptr)
 *   void reject(const string&)
 *   void forward_to(PromiseLike)
 */
class PromiseLike { };

/* shared_ptr around a PromiseState */
template <typename Result> class Promise;

namespace promise {

/* Can be used to pack callbacks for a promise, for use in monad syntax */
template <typename Range, typename Domain> class callback_pack;

}

}
