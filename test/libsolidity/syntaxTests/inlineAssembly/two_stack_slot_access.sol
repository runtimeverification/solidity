contract C {
    function f() pure external {
        function() external two_stack_slots;
        assembly {
            let x :=  two_stack_slots
        }
    }
}
// ----
// SyntaxError 1184: (99-157): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
