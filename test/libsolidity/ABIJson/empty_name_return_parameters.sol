contract test {
    function f(uint256 k) public returns (uint256) {
        return k;
    }
}
// ----
//     :test
// [
//   {
//     "inputs":
//     [
//       {
//         "internalType": "uint256",
//         "name": "k",
//         "type": "uint256"
//       }
//     ],
//     "name": "f",
//     "outputs":
//     [
//       {
//         "internalType": "uint256",
//         "name": "",
//         "type": "uint256"
//       }
//     ],
//     "stateMutability": "nonpayable",
//     "type": "function"
//   }
// ]
