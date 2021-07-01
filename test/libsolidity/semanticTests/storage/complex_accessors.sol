contract test {
    mapping(uint => bytes4) public to_string_map;
    mapping(uint => bool) public to_bool_map;
    mapping(uint => uint) public to_uint_map;
    mapping(uint => mapping(uint => uint)) public to_multiple_map;
    constructor() {
        to_string_map[42] = "24";
        to_bool_map[42] = false;
        to_uint_map[42] = 12;
        to_multiple_map[42][23] = 31;
    }
}
// ====
// compileViaYul: also
// ----
// to_string_map(uint): 42 -> 0x32340000
// to_bool_map(uint): 42 -> false
// to_uint_map(uint): 42 -> 12
// to_multiple_map(uint,uint): 42, 23 -> 31
