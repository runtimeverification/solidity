contract Test {
    function set(bytes memory _data, uint i)
        public
        returns (uint l, bytes1 c)
    {
        l = _data.length;
        c = _data[i];
    }
}
// ====
// compileViaYul: also
// ----
// set(bytes,uint): "abcdefgh", 0x03 -> 0x08, 0x64
