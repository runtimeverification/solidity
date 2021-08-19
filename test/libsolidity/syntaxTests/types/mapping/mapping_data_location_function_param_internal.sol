contract c {
    function f4(mapping(uint => uint) storage) pure internal {}
    function f5(mapping(uint => uint) memory) pure internal {}
}
// ----
// TypeError 4061: (93-121): Type mapping(uint => uint) is only valid in storage because it contains a (nested) mapping.
