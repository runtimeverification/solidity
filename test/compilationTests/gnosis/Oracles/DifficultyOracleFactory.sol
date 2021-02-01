pragma solidity >=0.0;
import "../Oracles/DifficultyOracle.sol";


/// @title Difficulty oracle factory contract - Allows to create difficulty oracle contracts
/// @author Stefan George - <stefan@gnosis.pm>
contract DifficultyOracleFactory {

    /*
     *  Events
     */
    event DifficultyOracleCreation(address indexed creator, DifficultyOracle difficultyOracle, uint256 blockNumber);

    /*
     *  Public functions
     */
    /// @dev Creates a new difficulty oracle contract
    /// @param blockNumber Target block number
    /// @return difficultyOracle Oracle contract
    function createDifficultyOracle(uint256 blockNumber)
        public
        returns (DifficultyOracle difficultyOracle)
    {
        difficultyOracle = new DifficultyOracle(blockNumber);
        emit DifficultyOracleCreation(msg.sender, difficultyOracle, blockNumber);
    }
}
