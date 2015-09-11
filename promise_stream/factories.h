#pragma once

namespace kaiu {

namespace promise {

template <typename Result, typename Datum, typename... Args>
using StreamFactory = std::function<PromiseStream<Result, Datum>(Args...)>;

}

}
