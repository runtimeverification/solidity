contract test {
    function f() public {}
    function g() public payable {}
}
// ----
//     :test
// [
//   {
//     "ieleName": "f()",
//     "inputs": [],
//     "name": "f",
//     "outputs": [],
//     "stateMutability": "nonpayable",
//     "type": "function"
//   },
//   {
//     "ieleName": "g()",
//     "inputs": [],
//     "name": "g",
//     "outputs": [],
//     "stateMutability": "payable",
//     "type": "function"
//   }
// ]
