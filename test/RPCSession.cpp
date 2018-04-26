/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.

	The Implementation originally from https://msdn.microsoft.com/en-us/library/windows/desktop/aa365592(v=vs.85).aspx
*/
/// @file RPCSession.cpp
/// Low-level IPC communication between the test framework and the Ethereum node.

#include <test/RPCSession.h>

#include <test/Options.h>

#include <libsolidity/interface/EVMVersion.h>

#include <libdevcore/CommonData.h>

#include <libdevcore/JSON.h>

#include <test/ExecutionFramework.h>

#include <string>
#include <stdio.h>
#include <thread>
#include <chrono>

using namespace std;
using namespace dev;

IPCSocket::IPCSocket(string const& _path): m_path(_path)
{
#if defined(_WIN32)
	m_socket = CreateFile(
		m_path.c_str(),   // pipe name
		GENERIC_READ |  // read and write access
		GENERIC_WRITE,
		0,              // no sharing
		NULL,           // default security attribute
		OPEN_EXISTING,  // opens existing pipe
		0,              // default attributes
		NULL);          // no template file

	if (m_socket == INVALID_HANDLE_VALUE)
		BOOST_FAIL("Error creating IPC socket object!");

#else
	if (_path.length() >= sizeof(sockaddr_un::sun_path))
		BOOST_FAIL("Error opening IPC: socket path is too long!");

	struct sockaddr_un saun;
	memset(&saun, 0, sizeof(sockaddr_un));
	saun.sun_family = AF_UNIX;
	strcpy(saun.sun_path, _path.c_str());

// http://idletechnology.blogspot.ca/2011/12/unix-domain-sockets-on-osx.html
//
// SUN_LEN() might be optimal, but it seemingly affects the portability,
// with at least Android missing this macro.  Just using the sizeof() for
// structure seemingly works, and would only have the side-effect of
// sending larger-than-required packets over the socket.  Given that this
// code is only used for unit-tests, that approach seems simpler.
#if defined(__APPLE__)
	saun.sun_len = sizeof(struct sockaddr_un);
#endif //  defined(__APPLE__)

	if ((m_socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
		BOOST_FAIL("Error creating IPC socket object");

	if (connect(m_socket, reinterpret_cast<struct sockaddr const*>(&saun), sizeof(struct sockaddr_un)) < 0)
	{
		close(m_socket);
		BOOST_FAIL("Error connecting to IPC socket: " << _path);
	}
#endif
}

string IPCSocket::sendRequest(string const& _req)
{
#if defined(_WIN32)
	// Write to the pipe.
	DWORD cbWritten;
	BOOL fSuccess = WriteFile(
		m_socket,               // pipe handle
		_req.c_str(),           // message
		_req.size(),            // message length
		&cbWritten,             // bytes written
		NULL);                  // not overlapped

	if (!fSuccess || (_req.size() != cbWritten))
		BOOST_FAIL("WriteFile to pipe failed");

	// Read from the pipe.
	DWORD cbRead;
	fSuccess = ReadFile(
		m_socket,          // pipe handle
		m_readBuf,         // buffer to receive reply
		sizeof(m_readBuf), // size of buffer
		&cbRead,           // number of bytes read
		NULL);             // not overlapped

	if (!fSuccess)
		BOOST_FAIL("ReadFile from pipe failed");

	return string(m_readBuf, m_readBuf + cbRead);
#else
	if (send(m_socket, _req.c_str(), _req.length(), 0) != (ssize_t)_req.length())
		BOOST_FAIL("Writing on IPC failed.");

	auto start = chrono::steady_clock::now();
	ssize_t ret;
	do
	{
		ret = recv(m_socket, m_readBuf, sizeof(m_readBuf), 0);
		// Also consider closed socket an error.
		if (ret < 0)
			BOOST_FAIL("Reading on IPC failed.");
	}
	while (
		ret == 0 &&
		chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start).count() < m_readTimeOutMS
	);

	if (ret == 0)
		BOOST_FAIL("Timeout reading on IPC.");

	return string(m_readBuf, m_readBuf + ret);
#endif
}

RPCSession& RPCSession::instance(const string& _path)
{
	static RPCSession session(_path);
	BOOST_REQUIRE_EQUAL(session.m_ipcSocket.path(), _path);
	return session;
}

string RPCSession::eth_getCode(string const& _address, string const& _blockNumber)
{
	return rpcCall("eth_getCode", { quote(_address), quote(_blockNumber) }).asString();
}

string RPCSession::eth_getTimestamp(string const& _blockNumber)
{
	// NOTE: to_string() converts bool to 0 or 1
	return rpcCall("eth_getBlockByNumber", { quote(_blockNumber), "false" })["timestamp"].asString();
}

RPCSession::TransactionReceipt RPCSession::eth_getTransactionReceipt(string const& _transactionHash)
{
	TransactionReceipt receipt;
	Json::Value const result = rpcCall("eth_getTransactionReceipt", { quote(_transactionHash) });
	BOOST_REQUIRE(!result.isNull());
	receipt.contractAddress = result["contractAddress"].asString();
	receipt.gasUsed = result["gasUsed"].asString();
	receipt.status = result["status"].asString();
	receipt.blockNumber = result["blockNumber"].asString();
	for (auto const& log: result["logs"])
	{
		LogEntry entry;
		entry.address = log["address"].asString();
		entry.data = log["data"].asString();
		for (auto const& topic: log["topics"])
			entry.topics.push_back(topic.asString());
		receipt.logEntries.push_back(entry);
	}
	return receipt;
}

string RPCSession::iele_sendTransaction(TransactionData const& _td)
{
	return iele_sendTransaction(_td.toJson());
}

string RPCSession::iele_sendTransaction(string const& _transaction)
{
	return rpcCall("iele_sendTransaction", { _transaction }).asString();
}

vector<string> RPCSession::iele_call(TransactionData const& _td)
{
	Json::Value rawOutput = rpcCall("iele_call", { _td.toJson(), quote("latest") });
	vector<string> result;
        for (auto const& output : rawOutput) {
		result.push_back(output.asString());
	}
	return result;
}

string RPCSession::eth_getBalance(string const& _address, string const& _blockNumber)
{
	string address = (_address.length() == 20) ? "0x" + _address : _address;
	return rpcCall("eth_getBalance", { quote(address), quote(_blockNumber) }).asString();
}

h256 const EmptyTrie("0x56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421");

bool RPCSession::eth_isStorageEmpty(string const& _address, string const& _blockNumber)
{
	string address = (_address.length() == 20) ? "0x" + _address : _address;
	string result = rpcCall("eth_getStorageRoot", { quote(address), quote(_blockNumber) }).asString();
	return h256(result) == EmptyTrie;
}

string RPCSession::personal_newAccount()
{
	string addr = rpcCall("personal_newAccount", { quote("") }).asString();
	BOOST_REQUIRE(rpcCall("personal_unlockAccount", { quote(addr), quote(""), to_string(100000) }));
	BOOST_TEST_MESSAGE("Created account " + addr);
	return addr;
}

void RPCSession::test_rewindToBlock(size_t _blockNr)
{
	BOOST_REQUIRE(rpcCall("test_rewindToBlock", { to_string(_blockNr) }) == true);
}

void RPCSession::test_mineBlocks(int _number)
{
	BOOST_REQUIRE(rpcCall("test_mineBlocks", { to_string(_number) }, true) == true);
}

void RPCSession::test_modifyTimestamp(size_t _timestamp)
{
	BOOST_REQUIRE(rpcCall("test_modifyTimestamp", { to_string(_timestamp) }) == true);
}

bool RPCSession::miner_setEtherbase(string const& _address)
{
	return rpcCall("miner_setEtherbase", { quote(_address) }).asBool();
}

void RPCSession::test_setBalance(vector<string> _accounts, string _balance) {
	string forks;
	if (test::Options::get().evmVersion() >= solidity::EVMVersion::tangerineWhistle())
		forks += "\"EIP150ForkBlock\": \"0x00\",\n";
	if (test::Options::get().evmVersion() >= solidity::EVMVersion::spuriousDragon())
		forks += "\"EIP158ForkBlock\": \"0x00\",\n";
	if (test::Options::get().evmVersion() >= solidity::EVMVersion::byzantium())
		forks += "\"byzantiumForkBlock\": \"0x00\",\n";
	if (test::Options::get().evmVersion() >= solidity::EVMVersion::constantinople())
		forks += "\"constantinopleForkBlock\": \"0x00\",\n";
	static string const c_configString = R"(
	{
		"sealEngine": "NoProof",
		"params": {
			"accountStartNonce": "0x00",
			"maximumExtraDataSize": "0x1000000",
			"blockReward": "0x",
			"allowFutureBlocks": true,
			)" + forks + R"(
			"homesteadForkBlock": "0x00"
		},
		"genesis": {
			"author": "0000000000000010000000000000000000000000",
			"timestamp": "0x00",
			"parentHash": "0x0000000000000000000000000000000000000000000000000000000000000000",
			"extraData": "0x",
			"gasLimit": "0x1000000000000"
		},
		"accounts": {}
	}
	)";

	Json::Value config;
	BOOST_REQUIRE(jsonParseStrict(c_configString, config));
	for (auto const& account: _accounts)
		config["accounts"][account]["wei"] = _balance;

	BOOST_REQUIRE(rpcCall("test_setChainParams", {jsonCompactPrint(config)}) == true);
}

Json::Value RPCSession::rpcCall(string const& _methodName, vector<string> const& _args, bool _canFail)
{
	string request = "{\"jsonrpc\":\"2.0\",\"method\":\"" + _methodName + "\",\"params\":[";
	for (size_t i = 0; i < _args.size(); ++i)
	{
		request += _args[i];
		if (i + 1 != _args.size())
			request += ", ";
	}

	request += "],\"id\":" + to_string(m_rpcSequence) + "}";
	++m_rpcSequence;

	BOOST_TEST_MESSAGE("Request: " + request);
	string reply = m_ipcSocket.sendRequest(request);
	BOOST_TEST_MESSAGE("Reply: " + reply);

	Json::Value result;
	string errorMsg;
	if (!jsonParseStrict(reply, result, &errorMsg))
		BOOST_REQUIRE_MESSAGE(false, errorMsg);

	if (result.isMember("error"))
	{
		if (_canFail)
			return Json::Value();

		BOOST_FAIL("Error on JSON-RPC call: " + result["error"]["message"].asString());
	}
	return result["result"];
}

string const& RPCSession::accountCreate()
{
	m_accounts.push_back(personal_newAccount());
	return m_accounts.back();
}

string const& RPCSession::accountCreateIfNotExists(size_t _id)
{
	while ((_id + 1) > m_accounts.size())
		accountCreate();
	return m_accounts[_id];
}

RPCSession::RPCSession(const string& _path):
	m_ipcSocket(_path)
{
	accountCreate();
	// This will pre-fund the accounts create prior.
	test_setBalance(m_accounts, "0x100000000000000000000000000000000000000000");
}

string RPCSession::TransactionData::toJson() const
{
	Json::Value json;
	json["from"] = (from.length() == 20) ? "0x" + from : from;
	json["to"] = (to.length() == 20 || to == "") ? "0x" + to :  to;
	json["gas"] = gas;
	json["gasprice"] = gasPrice;
	json["value"] = value;
        if (to == "" || to == "0x") {
		json["contractCode"] = data;
	} else {
		json["function"] = function;
	}
	json["arguments"] = Json::arrayValue;
	for (auto const& arg : arguments) {
		json["arguments"].append(arg);
	}
	return jsonCompactPrint(json);
}
