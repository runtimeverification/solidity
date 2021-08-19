contract C {
    function f() pure public {
        assembly {
            swap0()
            swap1()
            swap2()
            swap3()
            swap4()
            swap5()
            swap6()
            swap7()
            swap8()
            swap9()
            swap10()
            swap11()
            swap12()
            swap13()
            swap14()
            swap15()
            swap16()
            swap32()
        }
    }
}
// ----
// SyntaxError 1184: (52-440): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
