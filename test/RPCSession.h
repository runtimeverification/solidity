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
*/
/** @file RPCSession.h
 * @author Dimtiry Khokhlov <dimitry@ethdev.com>
 * @date 2016
 */

#pragma once

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#endif

#include <json/value.h>

#include <boost/noncopyable.hpp>
#include <boost/test/unit_test.hpp>

#include <string>
#include <stdio.h>
#include <map>

#if defined(_WIN32)
class IPCSocket : public boost::noncopyable
{
public:
	explicit IPCSocket(std::string const& _path);
	std::string sendRequest(std::string const& _req);
	~IPCSocket() { CloseHandle(m_socket); }

	std::string const& path() const { return m_path; }

private:
	std::string m_path;
	HANDLE m_socket;
	TCHAR m_readBuf[512000];
};
#else
class IPCSocket: public boost::noncopyable
{
public:
	explicit IPCSocket(std::string const& _path);
	std::string sendRequest(std::string const& _req);
	~IPCSocket() { close(m_socket); }

	std::string const& path() const { return m_path; }

private:

	std::string m_path;
	int m_socket;
	/// Socket read timeout in milliseconds. Needs to be large because the key generation routine
	/// might take long.
	unsigned static constexpr m_readTimeOutMS = 300000;
	char m_readBuf[512000];
};
#endif

class RPCSession: public boost::noncopyable
{
public:
	struct TransactionData
	{
		std::string from;
		std::string to;
		std::string gas;
		std::string gasPrice;
		std::string value;
		std::string data;
		std::string function;
		std::vector<std::string> arguments;

                Json::Value toJson(RPCSession*) const;
	};

	struct LogEntry {
		std::string address;
		std::vector<std::string> topics;
		std::string data;
	};

	struct TransactionReceipt
	{
		std::string gasUsed;
		std::string contractAddress;
		std::string status;
		std::vector<LogEntry> logEntries;
		std::string blockNumber;
		std::vector<std::string> returnData;
	};

	static RPCSession& instance(std::string const& _path, std::string const& _walletId, std::string const& _privateFromAddr);

        std::string eth_blockNumber(void);
	std::string eth_getCode(std::string const& _address, std::string const& _blockNumber);
	std::string eth_getTimestamp(std::string const& _blockNumber);
	TransactionReceipt eth_getTransactionReceipt(std::string const& _transactionHash);
	std::string iele_sendTransaction(TransactionData const& _td);
	std::string iele_sendTransaction(Json::Value const& _transaction);
	std::vector<std::string> iele_call(TransactionData const& _td);
	std::string eth_getBalance(std::string const& _address, std::string const& _blockNumber);
	bool eth_isStorageEmpty(std::string const& _address, std::string const& _blockNumber);
	void test_rewindToBlock(size_t _blockNr);
	void test_modifyTimestamp(size_t _timestamp);
	void test_mineBlocks(int _number);

	bool miner_setEtherbase(std::string const& _address);

	std::string const& account(size_t _id) const { return m_accounts.at(_id); }
	std::string const& accountCreate();
	std::string const& accountCreateIfNotExists(size_t _id);
        std::string bech32Encode(std::string const&);
        std::string bech32Decode(std::string const&);

private:
	explicit RPCSession(std::string const& _path, std::string const& _walletId, std::string const& _privateFromAddr);

	Json::Value rpcCall(std::string const& _methodName, std::vector<std::string> const& _args = std::vector<std::string>(), bool _canFail = false, bool passphrase = false);
	std::string personal_newAccount();
	void test_setBalance(std::vector<std::string> _accounts, std::string _balance);

	inline std::string quote(std::string const& _arg) { return "\"" + _arg + "\""; }
	/// Parse std::string replacing keywords to values
	void parseString(std::string& _string, std::map<std::string, std::string> const& _varMap);

	IPCSocket m_ipcSocket;
        std::string m_walletId;
        std::string m_privateFromAddr;
	size_t m_rpcSequence = 1;
	unsigned m_maxMiningTime = 6000000; // 600 seconds
	unsigned m_sleepTime = 10; // 10 milliseconds
	unsigned m_successfulMineRuns = 0;

	std::vector<std::string> m_accounts;
};

