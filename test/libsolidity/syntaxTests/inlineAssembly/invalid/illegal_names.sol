contract C {
    function f() public {
        // reserved function names
        assembly {
            function this() {
            }
            function super() {
            }
            function _() {
            }
        }

        // reserved names as function argument
        assembly {
            function a(this) {
            }
            function b(super) {
            }
            function c(_) {
            }
        }

        // reserved names as function return parameter
        assembly {
            function d() -> this {
            }
            function g() -> super {
            }
            function c() -> _ {
            }
        }

        // reserved names as variable declaration
        assembly {
            let this := 1
            let super := 1
            let _ := 1
        }
    }
}
// ----
//  SyntaxError 1184: (82-232): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
//  SyntaxError 1184: (289-442): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
//  SyntaxError 1184: (507-672): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
//  SyntaxError 1184: (732-828): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
