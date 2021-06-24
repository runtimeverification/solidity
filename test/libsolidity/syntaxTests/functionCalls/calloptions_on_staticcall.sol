contract C {
    function foo() pure internal {
        address(10).staticcall{value: 7, gas: 3}("");
    }
}
// ====
// EVMVersion: >=byzantium
// ----
// TypeError 6198: (56-78): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 2842: (56-96): Cannot set option "value" for staticcall.
