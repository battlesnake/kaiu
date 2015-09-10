#pragma once
#include <deque>
#include <queue>
#include <memory>
#include <functional>
#include <utility>
#include <type_traits>
#include <mutex>
#include "promise.h"
#include "self_managing.h"

#if defined(DEBUG)
#define SAFE_PROMISE_STREAMS
#endif

#include "promise_stream/fwd.h"
#include "promise_stream/traits.h"
#include "promise_stream/defs.h"
#include "promise_stream/factories.h"
#include "promise_stream/consumers.h"
#include "promise_stream/monad.h"
#include "promise_stream/state_base.h"
#include "promise_stream/state.h"

namespace kaiu {

using namespace std;

template <typename Result, typename Datum>
class PromiseStream : public PromiseLike {
	static_assert(!is_void<Result>::value, "Void promise streams are not supported");
	static_assert(!is_promise<Result>::value, "Promise of promise is probably not intended");
	static_assert(!is_void<Datum>::value, "Void promise streams are not supported");
	static_assert(!is_promise<Datum>::value, "Promise of promise is probably not intended");
public:
	using result_type = Result;
	using datum_type = Datum;
	/* Promise stream */
	PromiseStream();
	/* Copy/move/cast constructors */
	PromiseStream(PromiseStream<Result, Datum>&&) = default;
	PromiseStream(const PromiseStream<Result, Datum>&) = default;
	/* Assignment */
	PromiseStream<Result, Datum>& operator =(PromiseStream<Result, Datum>&&) = default;
	PromiseStream<Result, Datum>& operator =(const PromiseStream<Result, Datum>&) = default;
	/* Access promise state (then/except/finally/resolve/reject) */
	PromiseStreamState<Result, Datum> *operator ->() const;
protected:
	PromiseStream(shared_ptr<PromiseStreamState<Result, Datum>> const stream);
private:
	shared_ptr<PromiseStreamState<Result, Datum>> stream;
};

}

#ifndef promise_stream_tcc
#include "promise_stream.tcc"
#endif
