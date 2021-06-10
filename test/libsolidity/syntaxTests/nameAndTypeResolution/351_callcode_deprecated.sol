contract test {
    function f() pure public {
        address(0x12).callcode;
    }
}
// ----
// TypeError 6198: (55-77): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
