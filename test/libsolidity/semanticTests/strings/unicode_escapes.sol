contract C {
    function oneByteUTF8() public pure returns (string memory) {
        return "aaa\u0024aaa"; // usdollar
    }

    function twoBytesUTF8() public pure returns (string memory) {
        return "aaa\u00A2aaa"; // cent
    }

    function threeBytesUTF8() public pure returns (string memory) {
        return "aaa\u20ACaaa"; // euro
    }

    function combined() public pure returns (string memory) {
        return "\u0024\u00A2\u20AC";
    }
}
// ====
// compileViaYul: also
// compileToEwasm: also
// ----
// oneByteUTF8() -> "aaa$aaa"
// twoBytesUTF8() -> "aaa\xc2\xa2aaa"
// threeBytesUTF8() -> "aaa\xe2\x82\xacaaa"
// combined() -> "$\xc2\xa2\xe2\x82\xac"
