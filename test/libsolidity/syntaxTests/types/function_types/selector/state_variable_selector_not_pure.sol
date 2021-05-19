contract A {
    function() external public f;
}
contract B {
    function() external public g;
}

contract C is B {
    function() external public h;
    bytes4 constant s1 = h.selector;
    bytes4 constant s2 = B.g.selector;
    bytes4 constant s3 = this.h.selector;
}
// ----
// TypeError 2524: (176-186): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 8349: (176-186): Initial value for constant variable has to be compile-time constant.
// TypeError 2524: (213-225): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 8349: (213-225): Initial value for constant variable has to be compile-time constant.
// TypeError 2524: (252-267): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError 8349: (252-267): Initial value for constant variable has to be compile-time constant.
