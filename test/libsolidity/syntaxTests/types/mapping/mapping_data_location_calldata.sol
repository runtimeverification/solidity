contract c {
    mapping(uint => uint) y;
    function f() view public {
        mapping(uint => uint) calldata x = y;
        x;
    }
}
// ----
// TypeError 4061: (81-113): Type mapping(uint => uint) is only valid in storage because it contains a (nested) mapping.
