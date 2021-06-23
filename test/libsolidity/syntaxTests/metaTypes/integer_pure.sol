contract test {

    function viewAssignment() public view {
        int256 min = type(int256).min;
        min;
    }

    function assignment() public {
        int256 max = type(int256).max;
        max;
    }

}
// ----
// Warning 2018: (21-118): Function state mutability can be restricted to pure
// Warning 2018: (124-212): Function state mutability can be restricted to pure
