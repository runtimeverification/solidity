==== Source: a ====
contract A
{
	uint constant a = 42;
}
==== Source: b ====
import {A as b} from "a";
contract B {
    function f() public pure {
        assembly {
            let A := 1
        }
    }
}
// ----
// SyntaxError 1184: (b:78-121): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
