contract BinarySearch {
    /// Finds the position of _value in the sorted list _data.
    /// Note that "internal" is important here, because storage references only work for internal or private functions
    function find(uint[] storage _data, uint _value)
        internal
        returns (uint o_position)
    {
        return find(_data, 0, _data.length, _value);
    }

    function find(
        uint[] storage _data,
        uint _begin,
        uint _len,
        uint _value
    ) private returns (uint o_position) {
        if (_len == 0 || (_len == 1 && _data[_begin] != _value))
            return type(uint256).max; // failure
        uint halfLen = _len / 2;
        uint v = _data[_begin + halfLen];
        if (_value < v) return find(_data, _begin, halfLen, _value);
        else if (_value > v)
            return find(_data, _begin + halfLen + 1, halfLen - 1, _value);
        else return _begin + halfLen;
    }
}


contract Store is BinarySearch {
    uint[] data;

    function add(uint v) public {
        data.push(0);
        data[data.length - 1] = v;
    }

    function find(uint v) public returns (uint) {
        return find(data, v);
    }
}

// ====
// compileViaYul: also
// ----
// find(uint): 7 -> 0x00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
// add(uint): 7 ->
// find(uint): 7 -> 0
// add(uint): 11 ->
// add(uint): 17 ->
// add(uint): 27 ->
// add(uint): 31 ->
// add(uint): 32 ->
// add(uint): 66 ->
// add(uint): 177 ->
// find(uint): 7 -> 0
// find(uint): 27 -> 3
// find(uint): 32 -> 5
// find(uint): 176 -> 0x00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
// find(uint): 0 -> 0x00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
// find(uint): 400 -> 0x00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
