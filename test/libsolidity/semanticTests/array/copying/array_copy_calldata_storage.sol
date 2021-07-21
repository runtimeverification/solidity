contract c {
    uint[9] m_data;
    uint[] m_data_dyn;
    uint8[][] m_byte_data;
    function store(uint[9] calldata a, uint8[3][] calldata b) external returns (uint8) {
        m_data = a;
        m_data_dyn = a;
        m_byte_data = b;
        return b[3][1]; // note that access and declaration are reversed to each other
    }
    function retrieve() public returns (uint a, uint b, uint c, uint d, uint e, uint f, uint g) {
        a = m_data.length;
        b = m_data[7];
        c = m_data_dyn.length;
        d = m_data_dyn[7];
        e = m_byte_data.length;
        f = m_byte_data[3].length;
        g = m_byte_data[3][1];
    }
}
// ====
// compileViaYul: also
// ----
// store(uint[9],uint8[3][]): array 0 [ 21, 22, 23, 24, 25, 26, 27, 28, 29 ], refargs { 0x01, 0x04, 0x01, 0x02, 0x03, 0x0b, 0x0c, 0x0d, 0x15, 0x16, 0x17, 0x1f, 0x20, 0x21 } -> 32
// retrieve() -> 9, 28, 9, 28, 4, 3, 32
