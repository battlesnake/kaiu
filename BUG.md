Deadlocking in ParallelEventLoop::join

We need to ensure that we acquire mutexes in join() in the same order that a worker thread would acquire them.

Analyse mutex usage in worker threads, document acquisition order.

Replace all spinlocks with mutexes also (UserspaceSpinlock) since the runtime probably uses hybrid mutexes.

Fix the damn deadlock!!

Then read TODO.md, TODO-theory.md and add currying support to Task :)

~Mark
