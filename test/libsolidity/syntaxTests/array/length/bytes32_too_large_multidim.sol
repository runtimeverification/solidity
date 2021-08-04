contract C {
    bytes32[8**90][500] ids;
}
// ----
// TypeError 7676: (0-43): Contract requires too much storage.
// TypeError 1534: (17-40): Type too large for storage.
