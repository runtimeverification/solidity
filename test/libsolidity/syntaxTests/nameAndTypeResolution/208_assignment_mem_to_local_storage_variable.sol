contract C {
    uint[] data;
    function f(uint[] memory x) public {
        uint[] storage dataRef = data;
        dataRef = x;
    }
}
// ----
// TypeError 7407: (128-129): Type uint[] memory is not implicitly convertible to expected type uint[] storage pointer.
