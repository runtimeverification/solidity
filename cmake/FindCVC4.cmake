find_path(CVC4_INCLUDE_DIR cvc4/cvc4.h)
find_library(CVC4_LIBRARY NAMES cvc4 )
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CVC4 DEFAULT_MSG CVC4_LIBRARY CVC4_INCLUDE_DIR)
