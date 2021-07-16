pragma abicoder               v2;
struct S {
    uint16 x;
    bytes a;
    uint16 y;
    bytes b;
}
contract C {
    uint padding;
    S data;

    function f() public returns (bytes memory, bytes memory) {
        S memory x;
        x.x = 7;
        x.b = "1234567890123456789012345678901 1234567890123456789012345678901 123456789";
        x.a = "abcdef";
        x.y = 9;
        data = x;
        return (data.a, data.b);
    }
    function g() public returns (bytes memory, bytes memory) {
        S memory x;
        x.x = 7;
        x.b = "12345678923456789";
        x.a = "1234567890123456789012345678901 1234567890123456789012345678901 123456789";
        x.y = 9;
        data = x;
        return (data.a, data.b);
    }
    function h() public returns (bytes memory, bytes memory) {
        S memory x;
        data = x;
        return (data.a, data.b);
    }
}
// ====
// compileViaYul: also
// ----
// f() -> "abcdef", "1234567890123456789012345678901 1234567890123456789012345678901 123456789"
// g() -> "1234567890123456789012345678901 1234567890123456789012345678901 123456789", "12345678923456789"
// h() -> 0x00, 0x00
// storage: empty
