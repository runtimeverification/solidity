==== Source: s1.sol ====
import {f as g, g as h} from "s2.sol";
function f() pure returns (uint256) { return h() - g(); }
==== Source: s2.sol ====
import {f as h} from "s1.sol";
function f() pure returns (uint256) { return 2; }
function g() pure returns (uint256) { return 4; }
