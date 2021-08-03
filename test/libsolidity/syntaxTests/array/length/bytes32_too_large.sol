contract C {
    bytes32[8**90] ids;
}
// ----
// TypeError 7676: (0-38): Contract requires too much storage.
// TypeError 1534: (17-35): Type too large for storage.
