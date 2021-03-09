contract c {
    struct Data {
        uint x;
        uint y;
    }
    Data[] data;
    uint[] ids;

    function setIDStatic(uint id) public {
        ids[2] = id;
    }

    function setID(uint index, uint id) public {
        ids[index] = id;
    }

    function setData(uint index, uint x, uint y) public {
        data[index].x = x;
        data[index].y = y;
    }

    function getID(uint index) public returns (uint) {
        return ids[index];
    }

    function getData(uint index) public returns (uint x, uint y) {
        x = data[index].x;
        y = data[index].y;
    }

    function getLengths() public returns (uint l1, uint l2) {
        l1 = data.length;
        l2 = ids.length;
    }

    function setLengths(uint l1, uint l2) public {
        while (data.length < l1) data.push();
        while (ids.length < l2) ids.push();
    }
}

// ====
// compileViaYul: also
// ----
// getLengths() -> 0, 0
// setLengths(uint,uint): 48, 49 ->
// getLengths() -> 48, 49
// setIDStatic(uint): 11 ->
// getID(uint): 2 -> 11
// setID(uint,uint): 7, 8 ->
// getID(uint): 7 -> 8
// setData(uint,uint,uint): 7, 8, 9 ->
// setData(uint,uint,uint): 8, 10, 11 ->
// getData(uint): 7 -> 8, 9
// getData(uint): 8 -> 10, 11
