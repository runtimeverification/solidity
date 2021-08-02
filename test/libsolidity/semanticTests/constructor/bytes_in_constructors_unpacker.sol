contract Test {
    uint public m_x;
    bytes public m_s;
    constructor(uint x, bytes memory s) {
        m_x = x;
        m_s = s;
    }
}
// ----
// constructor(): 7, "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz" ->
// m_x() -> 7
// m_s() -> "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
