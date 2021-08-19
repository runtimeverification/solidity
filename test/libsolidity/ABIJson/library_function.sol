contract X {}
library test {
    struct StructType { uint256 a; }
    function f(StructType storage b, uint256[] storage c, X d) public returns (uint256[] memory e, StructType storage f) { f = f; }
    function f1(uint256[] memory c, X d) public pure returns (uint256[] memory e) {  }
}
// ----
//     :X
// []
//
//
//     :test
// [
//   {
//     "inputs":
//     [
//       {
//         "internalType": "uint256[]",
//         "name": "c",
//         "type": "uint256[]"
//       },
//       {
//         "internalType": "contract X",
//         "name": "d",
//         "type": "X"
//       }
//     ],
//     "name": "f1",
//     "outputs":
//     [
//       {
//         "internalType": "uint256[]",
//         "name": "e",
//         "type": "uint256[]"
//       }
//     ],
//     "stateMutability": "pure",
//     "type": "function"
//   }
// ]
