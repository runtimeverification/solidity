contract c {
    function f1(mapping(uint => uint)[] calldata) pure external {}
}
// ----
// TypeError 4103: (29-61): Types containing (nested) mappings can only be parameters or return variables of internal or library functions.
// TypeError 4061: (29-61): Type mapping(uint => uint)[] is only valid in storage because it contains a (nested) mapping.
