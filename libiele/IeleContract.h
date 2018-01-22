#include "llvm/ADT/simple_ilist.h"

namespace dev {
namespace iele {

class IeleFunction;

class IeleContract {
private:
  llvm::simple_ilist<IeleFunction> Functions;
};

} // end namespace iele
} // end namespace dev
