TODO
====

 * Remove all `using namespace` declarations from .h / .tcc files

 * Filesystem, socket, HTTP libraries, exposing Task/TaskStream factories.

 * Write a coding standard, and make the code fit it (currently very inconsistent).

 * Stream operator >> : eg `params >>= stream_factory >> data_func >>= completion_func/error_handler`
