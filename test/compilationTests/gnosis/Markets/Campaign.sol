pragma solidity ^0.4.11;
import "../Events/Event.sol";
import "../Markets/StandardMarketFactory.sol";
import "../Utils/Math.sol";


/// @title Campaign contract - Allows to crowdfund a market
/// @author Stefan George - <stefan@gnosis.pm>
contract Campaign {
    using Math for *;

    /*
     *  Events
     */
    event CampaignFunding(address indexed sender, uint256 funding);
    event CampaignRefund(address indexed sender, uint256 refund);
    event MarketCreation(Market indexed market);
    event MarketClosing();
    event FeeWithdrawal(address indexed receiver, uint256 fees);

     /*
     *  Constants
     */
    uint24 public constant FEE_RANGE = 1000000; // 100%

    /*
     *  Storage
     */
    Event public eventContract;
    MarketFactory public marketFactory;
    MarketMaker public marketMaker;
    Market public market;
    uint24 public fee;
    uint256 public funding;
    uint256 public deadline;
    uint256 public finalBalance;
    mapping (address => uint256) public contributions;
    Stages public stage;

    enum Stages {
        AuctionStarted,
        AuctionSuccessful,
        AuctionFailed,
        MarketCreated,
        MarketClosed
    }

    /*
     *  Modifiers
     */
    modifier atStage(Stages _stage) {
        // Contract has to be in given stage
        require(stage == _stage);
        _;
    }

    modifier timedTransitions() {
        if (stage == Stages.AuctionStarted && deadline < now)
            stage = Stages.AuctionFailed;
        _;
    }

    /*
     *  Public functions
     */
    /// @dev Constructor validates and sets campaign properties
    /// @param _eventContract Event contract
    /// @param _marketFactory Market factory contract
    /// @param _marketMaker Market maker contract
    /// @param _fee Market fee
    /// @param _funding Initial funding for market
    /// @param _deadline Campaign deadline
    function Campaign(
        Event _eventContract,
        MarketFactory _marketFactory,
        MarketMaker _marketMaker,
        uint24 _fee,
        uint256 _funding,
        uint256 _deadline
    )
        public
    {
        // Validate input
        require(   address(_eventContract) != 0
                && address(_marketFactory) != 0
                && address(_marketMaker) != 0
                && _fee < FEE_RANGE
                && _funding > 0
                && now < _deadline);
        eventContract = _eventContract;
        marketFactory = _marketFactory;
        marketMaker = _marketMaker;
        fee = _fee;
        funding = _funding;
        deadline = _deadline;
    }

    /// @dev Allows to contribute to required market funding
    /// @param amount Amount of collateral tokens
    function fund(uint256 amount)
        public
        timedTransitions
        atStage(Stages.AuctionStarted)
    {
        uint256 raisedAmount = eventContract.collateralToken().balanceOf(this);
        uint256 maxAmount = funding.sub(raisedAmount);
        if (maxAmount < amount)
            amount = maxAmount;
        // Collect collateral tokens
        require(eventContract.collateralToken().transferFrom(msg.sender, this, amount));
        contributions[msg.sender] = contributions[msg.sender].add(amount);
        if (amount == maxAmount)
            stage = Stages.AuctionSuccessful;
        CampaignFunding(msg.sender, amount);
    }

    /// @dev Withdraws refund amount
    /// @return Refund amount
    function refund()
        public
        timedTransitions
        atStage(Stages.AuctionFailed)
        returns (uint256 refundAmount)
    {
        refundAmount = contributions[msg.sender];
        contributions[msg.sender] = 0;
        // Refund collateral tokens
        require(eventContract.collateralToken().transfer(msg.sender, refundAmount));
        CampaignRefund(msg.sender, refundAmount);
    }

    /// @dev Allows to create market after successful funding
    /// @return Market address
    function createMarket()
        public
        timedTransitions
        atStage(Stages.AuctionSuccessful)
        returns (Market)
    {
        market = marketFactory.createMarket(eventContract, marketMaker, fee);
        require(eventContract.collateralToken().approve(market, funding));
        market.fund(funding);
        stage = Stages.MarketCreated;
        MarketCreation(market);
        return market;
    }

    /// @dev Allows to withdraw fees from market contract to campaign contract
    /// @return Fee amount
    function closeMarket()
        public
        atStage(Stages.MarketCreated)
    {
        // Winning outcome should be set
        require(eventContract.isOutcomeSet());
        market.close();
        market.withdrawFees();
        eventContract.redeemWinnings();
        finalBalance = eventContract.collateralToken().balanceOf(this);
        stage = Stages.MarketClosed;
        MarketClosing();
    }

    /// @dev Allows to withdraw fees from campaign contract to contributor
    /// @return Fee amount
    function withdrawFees()
        public
        atStage(Stages.MarketClosed)
        returns (uint256 fees)
    {
        fees = finalBalance.mul(contributions[msg.sender]) / funding;
        contributions[msg.sender] = 0;
        // Send fee share to contributor
        require(eventContract.collateralToken().transfer(msg.sender, fees));
        FeeWithdrawal(msg.sender, fees);
    }
}
