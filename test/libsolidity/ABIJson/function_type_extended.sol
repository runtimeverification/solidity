contract test {
    function g(function(test) external returns (test[] memory) x) public {}
}
// ----
//     :test
// [
//   {
//     "ieleName": "g(function)",
//     "inputs":
//     [
//       {
//         "internalType": "function (contract test) external returns (contract test[])",
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
