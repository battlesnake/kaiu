Promise stream
--------------

	http_server(config)
		->stream(handle_connection)
		->except(handle_error);

	auto handle_connection = [] (Connection con) {
		using Headers = std::unordered_map<string, string>;
		con.read(...)
			->stream<State>(...)
			->then(...);
	};

Non-blocking asynchronous streaming built on top of
(Promise)[https://github.com/battlesnake/kaiu/blob/master/promise.md].

State
-----

Streaming operation can be stateful or stateless.  When stateless, the streaming
callback is called for each piece of data written to the stream, and the oldest
un-processed data is the only parameter.  The stream result is the result passed
from the producer via the resolve() method.

	Promise<StreamAction> consumer(Datum)
	StreamAction consumer(Datum)

	stream(consumer) → Promise<Result>

A stateful streaming operation is analogous to a "reduce" or "aggregate"
operation in STL/JavaScript/.NET;  a state object is initialized when the
callback is bound, and this state object is passed by reference to the stream
callback (in addition to the data).  The callback may mutate this state
object.  The resulting promise from a stateful streaming operation is a
`std::pair`, which contains the final state in addition to the stream result.

	Promise<StreamAction> consumer(State&, Datum)
	StreamAction consumer(State&, Datum)

	stream(consumer, StateArgs...) → Promise<pair<State, Result>>

The state object is initialized with `StateArgs...` constructor arguments.

Consumer
--------

The consumer may be called zero, one, or multiple times.

The consumer returns a StreamAction, or a promise which resolves to a
StreamAction.

 * Continue: keep streaming and consuming data
 * Discard: keep streaming, discard data (don't call consumer again)
 * Stop: abort streaming - discards any remaining data and instructs producer to
   stop producing.  If the producer does not honour this then Stop has the same
   effect as Discard.

If the consumer returns void, StreamAction::Continue is assumed.

For the producer to stop the operation, it should resolve/reject the
promise stream.  Unless the consumer requests Stop/Discard, it will be
called for all remaining unprocessed data.  The resulting promise will not be
resolved/rejected until all data has been processed or discarded.

If the consumer throws or returns a rejected promise, the stream will eventually
reject with that exception (which overrides any result set by the producer).
Any remaining/future data will be ignored as if the consumer had returned
StreamAction::Stop.

The producer can use the "is_stopping" method to see whether the consumer has
requested the producer to stop.

The "data_action" method should not be used, it is only exposed to allow
forwarding of promise streams, which is required for task_stream.

Grizzly details
---------------

### Internal state transitions

Promise stream states:

	   ┌───────────┬──────────┬──────────┬──────────┬──────────┬────────────┐
	   │  Name of  │   Data   │ Data in  │ Consume  │   Have   │  Promise   │
	   │   state   │ written  │  buffer  │ running  │  result  │ completed  │
	   ├———————————┼——————————┼——————————┼——————————┼——————————┼————————————┤
	 A │pending    │ no       │ (no)     │ (no)     │ no       │ (no)       │
	 B │streaming1 │ yes      │ *        │ *        │ no       │ (no)       │
	 C │streaming2 │ yes      │ yes      │ *        │ yes      │ (no)       │
	 D │streaming3 │ yes      │ no       │ yes      │ yes      │ (no)       │
	 E │completed  │ *        │ no       │ no       │ (yes)    │ completing │
	   └───────────┴──────────┴──────────┴──────────┴──────────┴────────────┘
	
	      * = don't care (any value)
	  (val) = value is implicit, enforced due to value of some other field

State transition graph:

	  A ──┬──▶ B ──▶ C ──▶ D ──┬──▶ E
	      │                    │
	      └──────────▶─────────┘

State descriptions/conditions:

 * A: pending  
   initial state, nothing done  

 * B: streaming1
   data written, stream has no result (not been resolved/rejected)  
   A→B: write (& no result)  

 * C: streaming2
   data written, stream has result, buffer contains data to be consumed  
   B→C: result (& written)  

 * D: streaming3
   data written, stream has result, buffer is empty, consumer is running  
   C→D: buffer empty (& result & written)  

 * E: completed:
   stream has result, buffer is empty, consumer is not running  
   ∴ promise can be completed  
   A→E: result (& not written)  
   D→E: consumer not running (& buffer empty & result & written)  

### Flow within the state class

What happens when you act on a promise stream?

*Resolve/reject*

	           ╭──────╮
	   reject ─┤ lock ├─▶ do_reject ─┬─▶ set_action
	           ╰──────╯              │
	                                 ╰─┬─▶ set_stream_result ──▶ update_state
	           ╭──────╮                │
	  resolve ─┤ lock ├─▶ do_resolve───╯
	           ╰──────╯

*Write*

	         ╭──────╮
	  write ─┤ lock ├─┬─▶ set_stream_has_been_written_to ──▶ update_state
	         ╰──────╯ │
	                  ╰─▶ process_data ─▶ ···

*Stream*

	                                               ╭──────╮
	  stream ─¹─▶ do_stream ──▶ set_data_callback ─┤ lock ├─╮
	                                               ╰──────╯ │
	    ╭───────────────────────────────────────────────────╯
	    │
	    ╰─┬─▶ set_data_callback_assigned
	      │
	      ╰─▶ process_data ─▶ ···

  ¹ various forwardings depending on parameter types

*process_data*

	  ╭──────╮
	  │ lock ├─▶ ··· ──▶ process_data ──▶ take_data? ─────▶ set_buffer_is_empty
	  ╰──────╯                             ╷         false
	                                       │ true
	                                       ▼
	                               call_data_callback
	                                       ╷
	                                       │
	                                       ▼
	                         set_consumer_is_running(true) ──▶ update_state
	                                       ╷
	                                       │
	    (consumer might )                  ▼
	    (be asynchronous)           ╭──(consumer)──╮
	                       resolved │              │ rejected
	                               ╭┴──────────────┴╮
	                               │ reacquire lock │
	                               │    if async    │
	                               ╰┬──────────────┬╯
	                                │              │
	                                ▼              ▼
	                           set_action      do_reject ──▶ ··· ──▶ update_state
	                                ╷              ╷
	                                │              │
	                                ▼              ▼
	                         set_consumer_is_running(false) ──▶ update_state
	                                ╷
	                                │
	                                ▼
	                           process_data
	                                ╷
	                                │
	                                ▼
	                               ···


