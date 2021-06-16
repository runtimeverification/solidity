contract C {
    receive() external payable { msg.data; }
}
// ----
// TypeError 2699: (46-54): msg.data is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
