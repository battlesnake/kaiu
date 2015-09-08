#pragma once

namespace kaiu {

namespace promise {

/*
 * Returns a Promise<tuple> that resolves to a tuple of results when all
 * given promises have resolved, or which rejects if any of them reject
 * (without waiting for the others to complete).
 *
 * Think of this as a map() operation over a possibly heterogenous ordered set
 * of promises, which transforms the set into a tuple containing the results of
 * the promises.
 */
template <typename... Result>
Promise<tuple<typename decay<Result>::type...>> combine(Promise<Result>&&... promise);

/*
 * Takes an iterable of homogenous promises and returns a single promise that
 * resolves to a vector containing the results of the given promises.
 *
 * Think of this as a map() operation over a homogenous ordered set of promises,
 * transforming the set into an vector containing the results of the promises.
 *
 * Using std::vector for result rather than std::array, as the latter does not
 * support move semantics.
 */
template <typename It, typename Result = typename remove_cvr<typename It::value_type::result_type>::type>
Promise<vector<Result>> combine(It first, It last, const size_t size);

template <typename It, typename Result = typename remove_cvr<typename It::value_type::result_type>::type>
Promise<vector<Result>> combine(It first, It last);

template <typename List, typename Result = typename remove_cvr<typename List::value_type::result_type>::type>
Promise<vector<Result>> combine(List promises);

}

}
