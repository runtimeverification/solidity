contract C {
    function f() public returns (uint) {
        return uint(this.f);
    }
}
// ----
// TypeError: (69-81): Explicit type conversion not allowed from "function () external returns (uint)" to "uint".
