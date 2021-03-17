contract C {
    function f(bool _b) public returns (uint) {
        if (_b) return 1;
        else return 0;
    }

    function g(bool _in) public returns (bool _out) {
        _out = _in;
    }
}
// ====
// ABIEncoderV1Only: true
// ----
// f(bool): 0x0 -> 0x0
// f(bool): 0x1 -> 0x1
// f(bool): 0x2 ->
// f(bool): 0x3 ->
// f(bool): 0xff ->
// g(bool): 0x0 -> 0x0
// g(bool): 0x1 -> 0x1
// g(bool): 0x2 ->
// g(bool): 0x3 ->
// g(bool): 0xff ->
