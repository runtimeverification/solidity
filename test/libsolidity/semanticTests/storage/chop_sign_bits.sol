contract Test {
    //int16[] public x = [-1, -2];
    int16[] public x;
    function x_test(uint i) public returns (int16) {
      x = [-1, -2];
      return x[i];
    }
    //int16[2] public y = [-5, -6];
    int16[2] public y;
    function y_test(uint i) public returns (int16) {
      y = [-5, -6];
      return y[i];
    }
    int16 z;
    function f() public returns (int16[] memory) {
        int8[] memory t = new int8[](2);
        t[0] = -3;
        t[1] = -4;
        x = t;
        return x;
    }
    function g() public returns (int16[2] memory) {
        int8[2] memory t = [-3, -4];
        y = t;
        return y;
    }
    function h(int8 t) public returns (int16) {
        z = t;
        return z;
    }
}
// ====
// compileViaYul: also
// ----
// x_test(uint): 0 -> -1
// x_test(uint): 1 -> -2
// y_test(uint): 0 -> -5
// y_test(uint): 1 -> -6
// f() -> dynarray 16 [ -3, -4 ]
// g() -> array 16 [ -3, -4 ]
// h(int8): -10 -> -10
