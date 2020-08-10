#pragma once

namespace Kokkos {

template <class Namer, class BaseSpace, class DefaultExecutionSpace = void,
          bool SharesAccessWithBase = true>
class LogicalMemorySpace;

}
