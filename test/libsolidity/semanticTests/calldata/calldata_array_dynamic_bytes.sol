pragma abicoder               v2;


contract C {
    function f1(bytes[1] calldata a)
        external
        returns (uint, uint256, uint256, uint256)
    {
        return (a[0].length, uint8(a[0][0]), uint8(a[0][1]), uint8(a[0][2]));
    }

    function f2(bytes[1] calldata a, bytes[1] calldata b)
        external
        returns (uint, uint256, uint256, uint256, uint, uint256, uint256)
    {
        return (
            a[0].length,
            uint8(a[0][0]),
            uint8(a[0][1]),
            uint8(a[0][2]),
            b[0].length,
            uint8(b[0][0]),
            uint8(b[0][1])
        );
    }

    function g1(bytes[2] calldata a)
        external
        returns (
            uint,
            uint256,
            uint256,
            uint256,
            uint,
            uint256,
            uint256,
            uint256
        )
    {
        return (
            a[0].length,
            uint8(a[0][0]),
            uint8(a[0][1]),
            uint8(a[0][2]),
            a[1].length,
            uint8(a[1][0]),
            uint8(a[1][1]),
            uint8(a[1][2])
        );
    }

    function g2(bytes[] calldata a) external returns (uint[8] memory) {
        return [
            a.length,
            a[0].length,
            uint8(a[0][0]),
            uint8(a[0][1]),
            a[1].length,
            uint8(a[1][0]),
            uint8(a[1][1]),
            uint8(a[1][2])
        ];
    }
}

// found expectation comments:
// same offset for both arrays @ ABI_CHECK(

// ====
// compileViaYul: false
// ----
// f1(bytes[1]): refargs { "\x01\x02\x03" } -> 0x03, 0x01, 0x02, 0x03
// f2(bytes[1],bytes[1]): refargs { "\x01\x02\x03" }, refargs { "\x01\x02" } -> 0x03, 0x01, 0x02, 0x03, 0x02, 0x01, 0x02
// g1(bytes[2]): refargs { "\x01\x02\x03", "\x04\x05\x06" } -> 0x03, 0x01, 0x02, 0x03, 0x03, 0x04, 0x05, 0x06
// g1(bytes[2]): refargs { "\x01\x02\x03", "\x01\x02\x03" } -> 0x03, 0x01, 0x02, 0x03, 0x03, 0x01, 0x02, 0x03
// g2(bytes[]): refargs { 0x01, 0x02, "\x01\x02", "\x04\x05\x06" } -> array 0 [ 0x02, 0x02, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 ]
