contract test {
    function f(uint[] calldata seq) external pure returns (uint) {
        uint i = 0;
        uint sum = 0;
        while (i < seq.length)
        {
            uint idx = i;
            if (idx >= 10) break;
            uint x = seq[idx];
            if (x >= 1000) {
                uint n = i + 1;
                i = n;
                continue;
            }
            else {
                uint y = sum + x;
                sum = y;
            }
            if (sum >= 500) return sum;
            i++;
        }
        return sum;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f(uint[]): dynarray 0 [ 1000, 1, 2 ] -> 3
// f(uint[]): dynarray 0 [ 100, 500, 300 ] -> 600
// f(uint[]): dynarray 0 [ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 111 ] -> 55
