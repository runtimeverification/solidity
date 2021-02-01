pragma solidity >=0.0;
import "../Events/Event.sol";
import "../MarketMakers/MarketMaker.sol";


/// @title Abstract market contract - Functions to be implemented by market contracts
abstract contract Market {

    /*
     *  Events
     */
    event MarketFunding(uint256 funding);
    event MarketClosing();
    event FeeWithdrawal(uint256 fees);
    event OutcomeTokenPurchase(address indexed buyer, uint8 outcomeTokenIndex, uint256 outcomeTokenCount, uint256 cost);
    event OutcomeTokenSale(address indexed seller, uint8 outcomeTokenIndex, uint256 outcomeTokenCount, uint256 profit);
    event OutcomeTokenShortSale(address indexed buyer, uint8 outcomeTokenIndex, uint256 outcomeTokenCount, uint256 cost);

    /*
     *  Storage
     */
    address public creator;
    uint256 public createdAtBlock;
    Event public eventContract;
    MarketMaker public marketMaker;
    uint24 public fee;
    uint256 public funding;
    int256[] public netOutcomeTokensSold;
    Stages public stage;

    enum Stages {
        MarketCreated,
        MarketFunded,
        MarketClosed
    }

    /*
     *  Public functions
     */
    function fund(uint256 _funding) virtual public;
    function close() virtual public;
    function withdrawFees() virtual public returns (uint256);
    function buy(uint8 outcomeTokenIndex, uint256 outcomeTokenCount, uint256 maxCost) virtual public returns (uint256);
    function sell(uint8 outcomeTokenIndex, uint256 outcomeTokenCount, uint256 minProfit) virtual public returns (uint256);
    function shortSell(uint8 outcomeTokenIndex, uint256 outcomeTokenCount, uint256 minProfit) virtual public returns (uint256);
    function calcMarketFee(uint256 outcomeTokenCost) virtual public view returns (uint256);
}
