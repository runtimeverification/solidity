contract C {
    function foo(uint x) public
    {
        // Used to cause an ICE
        uint p = new uint[] = x;
    }
}
// ----
// TypeError 4247: (100-110): Expression has to be an lvalue.
// TypeError 7407: (113-114): Type uint is not implicitly convertible to expected type function (uint) pure returns (uint[] memory).
// TypeError 9574: (91-114): Type function (uint) pure returns (uint[] memory) is not implicitly convertible to expected type uint.
