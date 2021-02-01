contract C {
    function f() public returns (uint) {
        return uint(this.f);
    }
}
// ----
<<<<<<< ours
// TypeError: (69-81): Explicit type conversion not allowed from "function () external returns (uint)" to "uint".
=======
// TypeError 9640: (69-81): Explicit type conversion not allowed from "function () external returns (uint256)" to "uint256".
>>>>>>> theirs
