#pragma once

namespace kaiu {

namespace promise {

template <typename Datum>
using StatelessConsumer = std::function<Promise<StreamAction>(Datum)>;

template <typename State, typename Datum>
using StatefulConsumer = std::function<Promise<StreamAction>(State&, Datum)>;

}

}
