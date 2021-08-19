contract C {
    function f() pure public {
        abi.encodeWithSelector({selector:"abc"});
    }
}
// ----
// TypeError 2038: (52-74): abi.encodeWithSelector not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 2627: (52-92): Named arguments cannot be used for functions that take arbitrary parameters.
