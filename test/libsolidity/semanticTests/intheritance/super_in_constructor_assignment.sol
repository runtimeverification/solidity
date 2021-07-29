contract A {
    function f() public virtual returns (uint r) {
        return 1;
    }
}


contract B is A {
    function f() public virtual override returns (uint r) {
        function() internal returns (uint) x = super.f;
        return x() | 2;
    }
}


contract C is A {
    function f() public virtual override returns (uint r) {
        function() internal returns (uint) x = super.f;
        return x() | 4;
    }
}


contract D is B, C {
    uint data;

    constructor() {
        function() internal returns (uint) x = super.f;
        data = x() | 8;
    }

    function f() public override (B, C) returns (uint r) {
        return data;
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// f() -> 15
