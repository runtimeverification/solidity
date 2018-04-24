contract Factory {

    event ContractInstantiation(address sender, address instantiation);

    mapping(address => bool) public isInstantiation;
    mapping(address => address[]) public instantiations;

    /// @dev Returns number of instantiations by creator.
    /// @param creator Contract creator.
    /// @return Returns number of instantiations by creator.
    function getInstantiationCount(address creator)
        public
        constant
        returns (uint256)
    {
        return uint256(instantiations[creator].length);
    }

    /// @dev Registers contract in factory registry.
    /// @param instantiation Address of contract instantiation.
    function register(address instantiation)
        internal
    {
        isInstantiation[instantiation] = true;
        instantiations[msg.sender].push(instantiation);
        ContractInstantiation(msg.sender, instantiation);
    }
}
