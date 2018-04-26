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
// TypeError: (74-105): Type uint is not implicitly convertible to expected type int256.
// TypeError: (119-156): Type uint is not implicitly convertible to expected type int256.
// TypeError: (170-206): Operator ** not compatible with types int_const 4 and int_const -179...(302 digits omitted)...7216
// TypeError: (320-331): Operator ** not compatible with types int_const 2 and int_const 1000...(1226 digits omitted)...0000
// TypeError: (345-357): Operator ** not compatible with types int_const -2 and int_const 1000...(1226 digits omitted)...0000
// TypeError: (371-383): Operator ** not compatible with types int_const 2 and int_const -100...(1227 digits omitted)...0000
// TypeError: (397-410): Operator ** not compatible with types int_const -2 and int_const -100...(1227 digits omitted)...0000
// TypeError: (424-435): Type uint is not implicitly convertible to expected type int256.
// TypeError: (449-461): Type uint is not implicitly convertible to expected type int256.
// TypeError: (475-487): Type uint is not implicitly convertible to expected type int256.
// TypeError: (501-514): Type uint is not implicitly convertible to expected type int256.
// TypeError: (528-544): Operator ** not compatible with types int_const 1000...(1226 digits omitted)...0000 and int_const 1000...(1226 digits omitted)...0000
// TypeError: (528-544): Type int_const 1000...(1226 digits omitted)...0000 is not implicitly convertible to expected type int256.
// TypeError: (558-575): Operator ** not compatible with types int_const 1000...(1226 digits omitted)...0000 and int_const -100...(1227 digits omitted)...0000
// TypeError: (558-575): Type int_const 1000...(1226 digits omitted)...0000 is not implicitly convertible to expected type int256.
// TypeError: (589-606): Operator ** not compatible with types int_const -100...(1227 digits omitted)...0000 and int_const 1000...(1226 digits omitted)...0000
// TypeError: (589-606): Type int_const -100...(1227 digits omitted)...0000 is not implicitly convertible to expected type int256.
// TypeError: (620-638): Operator ** not compatible with types int_const -100...(1227 digits omitted)...0000 and int_const -100...(1227 digits omitted)...0000
// TypeError: (620-638): Type int_const -100...(1227 digits omitted)...0000 is not implicitly convertible to expected type int256.
