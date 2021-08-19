contract C {
    function f() public returns (bool success) {
        (success, ) = (address(0)).call{value: 30}("");
    }
}
// ----
// TypeError 6198: (84-101): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
