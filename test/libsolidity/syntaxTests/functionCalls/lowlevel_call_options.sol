contract C {
    function foo() internal {
        (bool success, ) = address(10).call{value: 7, gas: 3}("");
        success;
    }
}
// ----
// TypeError 6198: (70-108): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
