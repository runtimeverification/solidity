contract C {
    uint[8**90] ids;
}
// ----
// TypeError 7676: (0-35): Contract requires too much storage.
// TypeError 1534: (17-32): Type too large for storage.
