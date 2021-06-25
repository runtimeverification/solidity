contract c {
    function f() public pure {
        int256 a;
        a = 4 ** 4 ** 2 ** 4 ** 4 ** 4 ** 4;
        a = -4 ** 4 ** 2 ** 4 ** 4 ** 4 ** 4 ** 4;
        a = 4 ** (-(2 ** 4 ** 4 ** 4 ** 4 ** 4));
        a = 0 ** 1E1233; // fine
        a = 1 ** 1E1233; // fine
        a = -1 ** 1E1233; // fine
        a = 2 ** 1E1233;
        a = -2 ** 1E1233;
        a = 2 ** -1E1233;
        a = -2 ** -1E1233;
        a = 1E1233 ** 2;
        a = -1E1233 ** 2;
        a = 1E1233 ** -2;
        a = -1E1233 ** -2;
        a = 1E1233 ** 1E1233;
        a = 1E1233 ** -1E1233;
        a = -1E1233 ** 1E1233;
        a = -1E1233 ** -1E1233;
    }
}
// ----
// TypeError 2271: (89-105): Operator ** not compatible with types int_const 4 and int_const 1340...(147 digits omitted)...4096
// TypeError 2271: (74-105): Operator ** not compatible with types int_const 4 and int_const 4294967296
// TypeError 2271: (140-156): Operator ** not compatible with types int_const 4 and int_const 1340...(147 digits omitted)...4096
// TypeError 2271: (125-156): Operator ** not compatible with types int_const 4 and int_const 1157...(70 digits omitted)...9936
// TypeError 2271: (188-204): Operator ** not compatible with types int_const 4 and int_const 1340...(147 digits omitted)...4096
// TypeError 2271: (170-206): Operator ** not compatible with types int_const 4 and int_const -115...(71 digits omitted)...9936
// TypeError 2271: (320-331): Operator ** not compatible with types int_const 2 and int_const 1000...(1226 digits omitted)...0000
// TypeError 2271: (345-357): Operator ** not compatible with types int_const -2 and int_const 1000...(1226 digits omitted)...0000
// TypeError 2271: (371-383): Operator ** not compatible with types int_const 2 and int_const -100...(1227 digits omitted)...0000
// TypeError 2271: (397-410): Operator ** not compatible with types int_const -2 and int_const -100...(1227 digits omitted)...0000
// TypeError 7407: (424-435): Type uint is not implicitly convertible to expected type int256.
// TypeError 7407: (449-461): Type uint is not implicitly convertible to expected type int256.
// TypeError 2271: (475-487): Operator ** not compatible with types int_const 1000...(1226 digits omitted)...0000 and int_const -2. Precision of rational constants is limited to 4096 bits.
// TypeError 7407: (475-487): Type int_const 1000...(1226 digits omitted)...0000 is not implicitly convertible to expected type int256. Literal is too large to fit in int256.
// TypeError 2271: (501-514): Operator ** not compatible with types int_const -100...(1227 digits omitted)...0000 and int_const -2. Precision of rational constants is limited to 4096 bits.
// TypeError 7407: (501-514): Type int_const -100...(1227 digits omitted)...0000 is not implicitly convertible to expected type int256. Literal is too large to fit in int256.
// TypeError 2271: (528-544): Operator ** not compatible with types int_const 1000...(1226 digits omitted)...0000 and int_const 1000...(1226 digits omitted)...0000
// TypeError 7407: (528-544): Type int_const 1000...(1226 digits omitted)...0000 is not implicitly convertible to expected type int256. Literal is too large to fit in int256.
// TypeError 2271: (558-575): Operator ** not compatible with types int_const 1000...(1226 digits omitted)...0000 and int_const -100...(1227 digits omitted)...0000
// TypeError 7407: (558-575): Type int_const 1000...(1226 digits omitted)...0000 is not implicitly convertible to expected type int256. Literal is too large to fit in int256.
// TypeError 2271: (589-606): Operator ** not compatible with types int_const -100...(1227 digits omitted)...0000 and int_const 1000...(1226 digits omitted)...0000
// TypeError 7407: (589-606): Type int_const -100...(1227 digits omitted)...0000 is not implicitly convertible to expected type int256. Literal is too large to fit in int256.
// TypeError 2271: (620-638): Operator ** not compatible with types int_const -100...(1227 digits omitted)...0000 and int_const -100...(1227 digits omitted)...0000
// TypeError 7407: (620-638): Type int_const -100...(1227 digits omitted)...0000 is not implicitly convertible to expected type int256. Literal is too large to fit in int256.
