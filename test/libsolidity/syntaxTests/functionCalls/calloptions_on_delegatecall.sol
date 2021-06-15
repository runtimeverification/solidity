contract C {
    function foo() pure internal {
        address(10).delegatecall{value: 7, gas: 3}("");
    }
}
// ----
// TypeError 6198: (56-80): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 6189: (56-98): Cannot set option "value" for delegatecall.
