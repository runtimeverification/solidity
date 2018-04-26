contract C {
    uint[8**2048] ids;
}
// ----
// TypeError: (22-29): Invalid array length, expected integer literal or constant expression.
