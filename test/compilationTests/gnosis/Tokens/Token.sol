/// Implements ERC 20 Token standard: https://github.com/ethereum/EIPs/issues/20
pragma solidity >=0.0;


/// @title Abstract token contract - Functions to be implemented by token contracts
abstract contract Token {

    /*
     *  Events
     */
    event Transfer(address indexed from, address indexed to, uint256 value);
    event Approval(address indexed owner, address indexed spender, uint256 value);

    /*
     *  Public functions
     */
    function transfer(address to, uint256 value) virtual public returns (bool);
    function transferFrom(address from, address to, uint256 value) virtual public returns (bool);
    function approve(address spender, uint256 value) virtual public returns (bool);
    function balanceOf(address owner) virtual public view returns (uint256);
    function allowance(address owner, address spender) virtual public view returns (uint256);
    function totalSupply() virtual public view returns (uint256);
}
