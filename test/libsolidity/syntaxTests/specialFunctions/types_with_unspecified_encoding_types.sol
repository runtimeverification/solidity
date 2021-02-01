contract C {
    struct S { uint x; }
    S s;
    struct T { uint y; }
    T t;
    enum A { X, Y }
    function f() public pure {
        bytes memory a = abi.encodePacked(S, A, A.X, T, uint, uint[]);
        a;
    }
}
// ----
<<<<<<< ours
// Warning: (51-63): Defining empty structs is deprecated.
// TypeError: (141-195): Low-level calls are not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
// TypeError: (168-169): This type cannot be encoded.
// TypeError: (171-172): This type cannot be encoded.
// TypeError: (179-180): This type cannot be encoded.
// TypeError: (182-186): This type cannot be encoded.
// TypeError: (188-194): This type cannot be encoded.
=======
// TypeError 2056: (174-175): This type cannot be encoded.
// TypeError 2056: (177-178): This type cannot be encoded.
// TypeError 2056: (185-186): This type cannot be encoded.
// TypeError 2056: (188-192): This type cannot be encoded.
// TypeError 2056: (194-200): This type cannot be encoded.
>>>>>>> theirs
