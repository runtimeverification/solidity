contract test {
    uint[8] public data;
    uint[] public dynamicData;
    uint24[] public smallTypeData;
    struct st { uint a; uint[] finalArray; }
    mapping(uint => mapping(uint => st[5])) public multiple_map;

    constructor() {
        data[0] = 8;

        dynamicData.push();
        dynamicData.push();
        dynamicData.push(8);

        smallTypeData = new uint24[](128);
        smallTypeData[1] = 22;
        smallTypeData[127] = 2;

        multiple_map[2][1][2].a = 3;
        for (uint i = 0; i < 4; i++)
            multiple_map[2][1][2].finalArray.push();
        multiple_map[2][1][2].finalArray[3] = 5;
    }
}
// ====
// compileViaYul: also
// ----
// data(uint): 0 -> 8
// data(uint): 8 -> FAILURE, 255
// dynamicData(uint): 2 -> 8
// dynamicData(uint): 8 -> FAILURE, 255
// smallTypeData(uint): 1 -> 22
// smallTypeData(uint): 127 -> 2
// smallTypeData(uint): 128 -> FAILURE, 255
// multiple_map(uint,uint,uint): 2, 1, 2 -> 3
