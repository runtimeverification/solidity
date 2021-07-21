contract Test {
    function set(uint24[3][] memory _data, uint a, uint b)
        public
        returns (uint l, uint e)
    {
        l = _data.length;
        e = _data[a][b];
    }
}
// ====
// compileViaYul: also
// ----
// set(uint24[3][],uint,uint): refargs { 0x01, 0x06, 0x000001, 0x000002, 0x000003, 0x000004, 0x000005, 0x000006, 0x000007, 0x000008, 0x000009, 0x00000a, 0x00000b, 0x00000c, 0x00000d, 0x00000e, 0x00000f, 0x000010, 0x000011, 0x000012 }, 0x03, 0x02 -> 0x06, 0x0c
