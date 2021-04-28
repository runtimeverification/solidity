contract test {
    uint public data;
    bytes6 public name;
    bytes32 public a_hash;
    address public an_address;
    constructor() {
        data = 8;
        name = "Celina";
        a_hash = keccak256("\x7b");
        an_address = address(0x1337);
        super_secret_data = 42;
    }
    uint super_secret_data;
}
// ====
// compileViaYul: also
// compileToEwasm: also
// allowNonExistingFunctions: true
// ----
// data() -> 8
// name() -> 0x43656c696e61
// a_hash() -> 0x00a91eddf639b0b768929589c1a9fd21dcb0107199bdd82e55c5348018a1572f52
// an_address() -> 0x1337
// super_secret_data() -> FAILURE, 1
