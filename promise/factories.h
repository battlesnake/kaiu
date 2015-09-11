#pragma once

namespace kaiu {

namespace promise {

/* Construct a resolved promise */
template <typename Result, typename DResult = typename remove_cvr<Result>::type>
Promise<DResult> resolved(Result result);

/* Construct a rejected promise */
template <typename Result, typename DResult = typename remove_cvr<Result>::type>
Promise<DResult> rejected(std::exception_ptr error);

template <typename Result, typename DResult = typename remove_cvr<Result>::type>
Promise<DResult> rejected(const std::string& error);

/*
 * Convert function to promise factory
 *
 * Result(Args...) => Promise<Result>(Args...)
 *
 * Takes function func and returns a new function which when invoked, returns a
 * promise and calls func.  The return value of func is used to resolve the
 * promise.  If func throws, then the exception is caught and the promise is
 * rejected.
 *
 * Since func returns synchronously, the promise factory will evaluate func
 * BEFORE returning.  This is not a magic way to make synchronous functions
 * asynchronous, it just makes them useable in promise chains.  For a magic way
 * to make synchronous functions asynchronous, use std::thread or
 * kaiu::promise::task.
 *
 * Returns nullptr iff func==nullptr
 */
template <typename Result, typename... Args>
using Factory = std::function<Promise<Result>(Args...)>;

nullptr_t factory(nullptr_t);

template <typename Result, typename... Args>
Factory<Result, Args...> factory(Result (*func)(Args...));

template <typename Result, typename... Args>
Factory<Result, Args...> factory(std::function<Result(Args...)> func);

}

}
