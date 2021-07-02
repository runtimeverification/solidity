library L {
    function at(mapping(uint256 => uint256) storage a, uint256 i) internal view returns (uint256) {
        return a[i];
    }
}

contract C {
    using L for mapping(uint256 => uint256);

    mapping(uint256 => uint256) map;

    function mapValue(uint256 a) public returns (uint256) {
        map[42] = 0x24;
        map[66] = 0x66;

        return map.at(a);
    }
}
// ====
// compileViaYul: also
// ----
// mapValue(uint256): 42 -> 0x24
