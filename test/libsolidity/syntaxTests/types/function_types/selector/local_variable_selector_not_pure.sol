contract C {
    function f() public pure returns (bytes4) {
        function() external g;
        // Make sure g.selector is not considered pure:
        // If it was considered pure, this would emit a warning "Statement has no effect".
        g.selector;
    }
}
// ----
// TypeError 2524: (247-257): Member "selector" is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
