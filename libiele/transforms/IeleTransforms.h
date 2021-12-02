#pragma once

namespace solidity {
namespace iele {

class IeleContract;

namespace transform {

void deadCodeElimination(IeleContract &C);

} // end namespace transform
} // end namespace iele
} // end namespace solidity
