Kaiu
====

N59E25

Promises, parallel event-loops, segregated thread pools, functional fun, ...

All contents of this library are located within the namespace `kaiu`.

Documentation
-------------

The header files are commented.  Some components have their own README:

 * (Promise)[https://github.com/battlesnake/kaiu/blob/master/promise.md]

 * (Functional)[https://github.com/battlesnake/kaiu/blob/master/functional.md]

 * (Event loop)[https://github.com/battlesnake/kaiu/blob/master/event_loop.md]

 * (Task)[https://github.com/battlesnake/kaiu/blob/master/task.md]

 * (Assertion)[https://github.com/battlesnake/kaiu/blob/master/assertion.md]

 * (Tuple iteration)[https://github.com/battlesnake/kaiu/blob/master/tuple_iteration.md]

Building / running tests
------------------------

Requires g++, developed using version 5.2.0.

To build and run the tests:

	make tests

To build in release-mode (optimizations enabled, warnings disabled):

	make tests mode=release

To run a particular test, use the runner:

	./run_test [commands]

For a description of the commands, run:

	./run_test help:

For example, to build and run the promise test a leak check with valgrind:

	./run_test mem: promise
