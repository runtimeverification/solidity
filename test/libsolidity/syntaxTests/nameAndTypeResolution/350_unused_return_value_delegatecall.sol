contract test {
    function f() public {
        address(0x12).delegatecall("abc");
    }
}
// ----
// TypeError 6198: (50-76): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
