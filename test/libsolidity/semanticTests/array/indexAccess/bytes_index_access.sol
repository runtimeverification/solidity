contract c {
    bytes data;
    function direct(bytes calldata arg, uint index) external returns (uint) {
        return uint(uint8(arg[index]));
    }
    function storageCopyRead(bytes calldata arg, uint index) external returns (uint) {
        data = arg;
        return uint(uint8(data[index]));
    }
    function storageWrite() external returns (uint) {
        data = new bytes(35);
        data[31] = 0x77;
        data[32] = 0x14;

        data[31] = 0x01;
        data[31] |= 0x08;
        data[30] = 0x01;
        data[32] = 0x03;
        return uint(uint8(data[30])) * 0x100 | uint(uint8(data[31])) * 0x10 | uint(uint8(data[32]));
    }
}
// ====
// compileViaYul: also
// ----
// direct(bytes,uint): "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20\x21", 33 -> 0x21
// storageCopyRead(bytes,uint): "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20\x21", 33 -> 0x21
// storageWrite() -> 0x193
