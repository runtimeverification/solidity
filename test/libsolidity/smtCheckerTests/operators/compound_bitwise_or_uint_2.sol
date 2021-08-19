pragma experimental SMTChecker;
contract C {
    struct S {
        uint256[] x;
    }
    S s;
    function f(bool b) public {
        if (b)
            s.x[2] |= 1;
		// Removed because of Spacer nondeterminism.
        //assert(s.x[2] != 1);
    }
}
