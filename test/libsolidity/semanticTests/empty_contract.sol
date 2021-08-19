contract test {
}
// ====
// compileViaYul: also
// compileToEwasm: also
// allowNonExistingFunctions: true
// ----
// i_am_not_there() -> FAILURE, 0x01
