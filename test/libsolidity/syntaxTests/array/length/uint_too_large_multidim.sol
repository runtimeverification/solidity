contract C {
    uint[8**90][500] ids;
}
// ----
// TypeError 7676: (0-40): Contract requires too much storage.
// TypeError 1534: (17-37): Type too large for storage.
