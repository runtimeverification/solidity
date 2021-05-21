contract D {
    mapping (uint => uint) a;
    function foo() public view {
        mapping (uint => int) storage c = a;
    }
}
// ----
// TypeError 9574: (84-119): Type mapping(uint => uint) is not implicitly convertible to expected type mapping(uint => int).
