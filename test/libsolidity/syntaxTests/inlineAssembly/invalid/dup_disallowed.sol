contract C {
    function f() pure public {
        assembly {
            dup0()
            dup1()
            dup2()
            dup3()
            dup4()
            dup5()
            dup6()
            dup7()
            dup8()
            dup9()
            dup10()
            dup11()
            dup12()
            dup13()
            dup14()
            dup15()
            dup16()
            dup32()
        }
    }
}
// ----
//SyntaxError 1184: (52-422): Inline assembly is not supported in IELE. For more information, including potential workarounds, see README-IELE-SUPPORT.md
