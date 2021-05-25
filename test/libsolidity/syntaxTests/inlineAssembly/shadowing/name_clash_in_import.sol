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
            let b := 3
            let b.a := 4
        }
    }
}
// ----
// SyntaxError 1184: (b:78-146): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
