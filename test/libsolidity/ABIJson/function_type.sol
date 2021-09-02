contract test {
    function g(function(uint256) external returns (uint256) x) public {}
}
// ----
//     :test
// [
//   {
//     "ieleName": "g(function)",
//     "inputs":
//     [
//       {
//         "internalType": "function (uint256) external returns (uint256)",
//         "name": "x",
//         "type": "function"
//       }
//     ],
//     "name": "g",
//     "outputs": [],
//     "stateMutability": "nonpayable",
//     "type": "function"
//   }
// ]
