contract C {
    uint256[] storageArray;
    function pushEmpty(uint256 len) public {
        while(storageArray.length < len)
            storageArray.push();

        for (uint i = 0; i < len; i++)
            require(storageArray[i] == 0);
    }
}
// ====
// compileViaYul: also
// EVMVersion: >=petersburg
// ----
// pushEmpty(uint256): 128
// pushEmpty(uint256): 256
// pushEmpty(uint256): 32768 -> FAILURE, 5 # out-of-gas #
