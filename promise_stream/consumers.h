#pragma once

namespace kaiu {

namespace promise {

template <typename Datum>
using StatelessConsumer = function<Promise<StreamAction>(Datum)>;

template <typename State, typename Datum>
using StatefulConsumer = function<Promise<StreamAction>(State&, Datum)>;

}

}
