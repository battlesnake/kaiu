#pragma once

namespace kaiu {

/***
 * Typed promise state
 *
 * You will never use this class directly, instead use the Promise<T>, which
 * encapsulates a shareable PromiseState<T>.
 *
 * The PromiseState<T> is aggregated onto the Promise<T> via the indirect (->)
 * operator.
 */

template <typename Result>
class PromiseState : public PromiseStateBase {
public:
	static_assert(
		std::is_same<Result, typename remove_cvr<Result>::type>::value,
		"Type parameter for promise internal state must not be cv-qualified or a reference");
	/* "then" and "except" returning new value or next promise */
	template <typename NextResult = Result>
		using NextFunc = std::function<NextResult(Result)>;
	template <typename NextResult = Result>
		using ExceptFunc = std::function<NextResult(std::exception_ptr)>;
	/* "then" and "except" ending promise chain */
	using NextVoidFunc = NextFunc<void>;
	using ExceptVoidFunc = ExceptFunc<void>;
	/* "finally" doesn't return any value */
	using FinallyFunc = std::function<void()>;
	/* Default constructor */
	PromiseState() = default;
	using PromiseStateBase::PromiseStateBase;
	/* No copy/move constructors */
	PromiseState(PromiseState<Result>&&) = delete;
	PromiseState(const PromiseState<Result>&) = delete;
	/* Resolve */
	void resolve(Result result);
	/* Reject */
	using PromiseStateBase::reject;
	/* Forwards the result of this promise to another promise */
	template <typename NextPromise>
	void forward_to(NextPromise next);
	/* Bind a callback pack */
	template <typename Range>
	Promise<Range> then(const promise::callback_pack<Range, Result>);
	/* Then (callbacks return immediate value) */
	template <typename Next>
	using ThenResult = typename std::result_of<Next(Result)>::type;
	template <
		typename Next,
		typename NextResult = ThenResult<Next>,
		typename Except = ExceptFunc<NextResult>,
		typename Finally = FinallyFunc,
		typename = typename std::enable_if<
			!is_promise<NextResult>::value &&
			!std::is_void<NextResult>::value &&
			!is_callback_pack<Next>::value
		>::type>
	Promise<NextResult> then(
			Next next_func,
			Except except_func = nullptr,
			Finally finally_func = nullptr);
	/* Then (callbacks return promise) */
	template <
		typename Next,
		typename NextPromise = ThenResult<Next>,
		typename NextResult = typename NextPromise::result_type,
		typename Except = ExceptFunc<NextPromise>,
		typename Finally = FinallyFunc,
		typename = typename std::enable_if<
			is_promise<NextPromise>::value &&
			!is_callback_pack<Next>::value
		>::type>
	Promise<NextResult> then(
			Next next_func,
			Except except_func = nullptr,
			Finally finally_func = nullptr);
	/* Then (end promise chain) */
	template <
		typename Next,
		typename NextResult = ThenResult<Next>,
		typename Except = ExceptVoidFunc,
		typename Finally = FinallyFunc,
		typename = typename std::enable_if<
			std::is_void<NextResult>::value &&
			!is_callback_pack<Next>::value
		>::type>
	void then(
		Next next_func,
		Except except_func = nullptr,
		Finally finally_func = nullptr);
	/* Except */
	template <
		typename Except,
		typename NextPromise = typename std::result_of<Except(std::exception_ptr)>::type,
		typename NextResult = typename NextPromise::result_type,
		typename = typename std::enable_if<
			is_promise<NextPromise>::value
		>::type>
	Promise<NextResult> except(
		Except except_func)
			{ return then<NextFunc<Promise<NextResult>>>(nullptr, except_func); }
	template <
		typename Except,
		typename NextResult = typename std::result_of<Except(std::exception_ptr)>::type,
		typename = typename std::enable_if<
			!is_promise<NextResult>::value &&
			!std::is_void<NextResult>::value
		>::type>
	Promise<NextResult> except(
		Except except_func)
			{ return then<NextFunc<NextResult>>(nullptr, except_func); }
	/* Except (end promise chain) */
	template <
		typename Except,
		typename NextResult = typename std::result_of<Except(std::exception_ptr)>::type,
		typename = typename std::enable_if<
			std::is_void<NextResult>::value
		>::type>
	void except(
		Except except_func)
			{ then<NextVoidFunc>(nullptr, except_func); }
	/* Finally */
	template <typename Finally>
	Promise<Result> finally(
		Finally finally_func)
			{ return then<NextFunc<Result>>(nullptr, nullptr, finally_func); }
	/* Make terminator with finalizer */
	template <typename Finally>
	void finish(Finally finally_func)
		{ finally(finally_func)->finish(); }
	using PromiseStateBase::finish;
protected:
	/* Get/set promise result */
	void set_result(ensure_locked, Result&& value);
	Result get_result(ensure_locked);
private:
	Result result;
	/* Helper functions to pass current value onwards if no 'next' callback */
	template <typename NextResult,
		typename = typename std::enable_if<std::is_same<Result, NextResult>::value>::type>
	static NextResult forward_result(Result result);
	template <typename NextResult, int dummy = 0,
		typename = typename std::enable_if<!std::is_same<Result, NextResult>::value>::type>
	static NextResult forward_result(Result result);
	template <typename NextResult>
	static Promise<NextResult> default_next(Result result);
	template <typename NextResult>
	static Promise<NextResult> default_except(std::exception_ptr error);
	static void default_finally();
};

}
