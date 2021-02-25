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
/**
 * @date 2017
 * Unit tests for the metadata output.
 */

#include <test/Metadata.h>
#include <test/Common.h>
#include <libsolidity/interface/CompilerStack.h>
#include <libsolidity/interface/Version.h>
#include <libsolutil/SwarmHash.h>
#include <libsolutil/IpfsHash.h>
#include <libsolutil/JSON.h>

#include <boost/test/unit_test.hpp>

using namespace std;

namespace solidity::frontend::test
{

namespace
{
map<string, string> requireParsedCBORMetadata(bytes const& _bytecode)
{
	bytes cborMetadata = solidity::test::onlyMetadata(_bytecode);
	BOOST_REQUIRE(!cborMetadata.empty());
	std::optional<map<string, string>> tmp = solidity::test::parseCBORMetadata(cborMetadata);
	BOOST_REQUIRE(tmp);
	return *tmp;
}
}

BOOST_AUTO_TEST_SUITE(Metadata)

BOOST_AUTO_TEST_CASE(metadata_stamp)
{
	// Check that the metadata stamp is at the end of the runtime bytecode.
	char const* sourceCode = R"(
		pragma solidity >=0.0;
		pragma experimental __testOnlyAnalysis;
		contract test {
			function g(function(uint) external returns (uint) x) public {}
		}
	)";
	for (auto release: std::set<bool>{true, VersionIsRelease})
		for (auto metadataHash: set<CompilerStack::MetadataHash>{
			CompilerStack::MetadataHash::IPFS,
			CompilerStack::MetadataHash::Bzzr1,
			CompilerStack::MetadataHash::None
		})
		{
			CompilerStack compilerStack;
			compilerStack.overwriteReleaseFlag(release);
			compilerStack.setSources({{"", std::string(sourceCode)}});
			compilerStack.setEVMVersion(solidity::test::CommonOptions::get().evmVersion());
			compilerStack.setOptimiserSettings(solidity::test::CommonOptions::get().optimize);
			compilerStack.setMetadataHash(metadataHash);
			BOOST_REQUIRE_MESSAGE(compilerStack.compile(), "Compiling contract failed");
			bytes const& bytecode = compilerStack.object("test").bytecode;
			std::string const& metadata = compilerStack.metadata("test");
			BOOST_CHECK(solidity::test::isValidMetadata(metadata));

			auto const cborMetadata = requireParsedCBORMetadata(bytecode);
			if (metadataHash == CompilerStack::MetadataHash::None)
				BOOST_CHECK(cborMetadata.size() == 1);
			else
			{
				bytes hash;
				string hashMethod;
				if (metadataHash == CompilerStack::MetadataHash::IPFS)
				{
					hash = util::ipfsHash(metadata);
					BOOST_REQUIRE(hash.size() == 34);
					hashMethod = "ipfs";
				}
				else
				{
					hash = util::bzzr1Hash(metadata).asBytes();
					BOOST_REQUIRE(hash.size() == 32);
					hashMethod = "bzzr1";
				}

				BOOST_CHECK(cborMetadata.size() == 2);
				BOOST_CHECK(cborMetadata.count(hashMethod) == 1);
				BOOST_CHECK(cborMetadata.at(hashMethod) == util::toHex(hash));
			}

			BOOST_CHECK(cborMetadata.count("solc") == 1);
			if (release)
				BOOST_CHECK(cborMetadata.at("solc") == util::toHex(VersionCompactBytes));
			else
				BOOST_CHECK(cborMetadata.at("solc") == VersionStringStrict);
		}
}

BOOST_AUTO_TEST_CASE(metadata_stamp_experimental)
{
	// Check that the metadata stamp is at the end of the runtime bytecode.
	char const* sourceCode = R"(
		pragma solidity >=0.0;
		pragma experimental __test;
		contract test {
			function g(function(uint) external returns (uint) x) public {}
		}
	)";
	for (auto release: set<bool>{true, VersionIsRelease})
		for (auto metadataHash: set<CompilerStack::MetadataHash>{
			CompilerStack::MetadataHash::IPFS,
			CompilerStack::MetadataHash::Bzzr1,
			CompilerStack::MetadataHash::None
		})
		{
			CompilerStack compilerStack;
			compilerStack.overwriteReleaseFlag(release);
			compilerStack.setSources({{"", std::string(sourceCode)}});
			compilerStack.setEVMVersion(solidity::test::CommonOptions::get().evmVersion());
			compilerStack.setOptimiserSettings(solidity::test::CommonOptions::get().optimize);
			compilerStack.setMetadataHash(metadataHash);
			BOOST_REQUIRE_MESSAGE(compilerStack.compile(), "Compiling contract failed");
			bytes const& bytecode = compilerStack.object("test").bytecode;
			std::string const& metadata = compilerStack.metadata("test");
			BOOST_CHECK(solidity::test::isValidMetadata(metadata));

			auto const cborMetadata = requireParsedCBORMetadata(bytecode);
			if (metadataHash == CompilerStack::MetadataHash::None)
				BOOST_CHECK(cborMetadata.size() == 2);
			else
			{
				bytes hash;
				string hashMethod;
				if (metadataHash == CompilerStack::MetadataHash::IPFS)
				{
					hash = util::ipfsHash(metadata);
					BOOST_REQUIRE(hash.size() == 34);
					hashMethod = "ipfs";
				}
				else
				{
					hash = util::bzzr1Hash(metadata).asBytes();
					BOOST_REQUIRE(hash.size() == 32);
					hashMethod = "bzzr1";
				}

				BOOST_CHECK(cborMetadata.size() == 3);
				BOOST_CHECK(cborMetadata.count(hashMethod) == 1);
				BOOST_CHECK(cborMetadata.at(hashMethod) == util::toHex(hash));
			}

			BOOST_CHECK(cborMetadata.count("solc") == 1);
			if (release)
				BOOST_CHECK(cborMetadata.at("solc") == util::toHex(VersionCompactBytes));
			else
				BOOST_CHECK(cborMetadata.at("solc") == VersionStringStrict);
			BOOST_CHECK(cborMetadata.count("experimental") == 1);
			BOOST_CHECK(cborMetadata.at("experimental") == "true");
		}
}

BOOST_AUTO_TEST_CASE(metadata_relevant_sources)
{
	CompilerStack compilerStack;
	char const* sourceCodeA = R"(
		pragma solidity >=0.0;
		contract A {
			function g(function(uint) external returns (uint) x) public {}
		}
	)";
	char const* sourceCodeB = R"(
		pragma solidity >=0.0;
		contract B {
			function g(function(uint) external returns (uint) x) public {}
		}
	)";
	compilerStack.setSources({
		{"A", std::string(sourceCodeA)},
		{"B", std::string(sourceCodeB)},
	});
	compilerStack.setEVMVersion(solidity::test::CommonOptions::get().evmVersion());
	compilerStack.setOptimiserSettings(solidity::test::CommonOptions::get().optimize);
	BOOST_REQUIRE_MESSAGE(compilerStack.compile(), "Compiling contract failed");

	std::string const& serialisedMetadata = compilerStack.metadata("A");
	BOOST_CHECK(solidity::test::isValidMetadata(serialisedMetadata));
	Json::Value metadata;
	BOOST_REQUIRE(util::jsonParseStrict(serialisedMetadata, metadata));

	BOOST_CHECK_EQUAL(metadata["sources"].size(), 1);
	BOOST_CHECK(metadata["sources"].isMember("A"));
}

BOOST_AUTO_TEST_CASE(metadata_relevant_sources_imports)
{
	CompilerStack compilerStack;
	char const* sourceCodeA = R"(
		pragma solidity >=0.0;
		contract A {
			function g(function(uint) external returns (uint) x) public virtual {}
		}
	)";
	char const* sourceCodeB = R"(
		pragma solidity >=0.0;
		import "./A";
		contract B is A {
			function g(function(uint) external returns (uint) x) public virtual override {}
		}
	)";
	char const* sourceCodeC = R"(
		pragma solidity >=0.0;
		import "./B";
		contract C is B {
			function g(function(uint) external returns (uint) x) public override {}
		}
	)";
	compilerStack.setSources({
		{"A", std::string(sourceCodeA)},
		{"B", std::string(sourceCodeB)},
		{"C", std::string(sourceCodeC)}
	});
	compilerStack.setEVMVersion(solidity::test::CommonOptions::get().evmVersion());
	compilerStack.setOptimiserSettings(solidity::test::CommonOptions::get().optimize);
	BOOST_REQUIRE_MESSAGE(compilerStack.compile(), "Compiling contract failed");

	std::string const& serialisedMetadata = compilerStack.metadata("C");
	BOOST_CHECK(solidity::test::isValidMetadata(serialisedMetadata));
	Json::Value metadata;
	BOOST_REQUIRE(util::jsonParseStrict(serialisedMetadata, metadata));

	BOOST_CHECK_EQUAL(metadata["sources"].size(), 3);
	BOOST_CHECK(metadata["sources"].isMember("A"));
	BOOST_CHECK(metadata["sources"].isMember("B"));
	BOOST_CHECK(metadata["sources"].isMember("C"));
}

BOOST_AUTO_TEST_CASE(metadata_useLiteralContent)
{
	// Check that the metadata contains "useLiteralContent"
	char const* sourceCode = R"(
		pragma solidity >=0.0;
		contract test {
		}
	)";

	auto check = [](char const* _src, bool _literal)
	{
		CompilerStack compilerStack;
		compilerStack.setSources({{"", std::string(_src)}});
		compilerStack.setEVMVersion(solidity::test::CommonOptions::get().evmVersion());
		compilerStack.setOptimiserSettings(solidity::test::CommonOptions::get().optimize);
		compilerStack.useMetadataLiteralSources(_literal);
		BOOST_REQUIRE_MESSAGE(compilerStack.compile(), "Compiling contract failed");
		string metadata_str = compilerStack.metadata("test");
		Json::Value metadata;
		util::jsonParseStrict(metadata_str, metadata);
		BOOST_CHECK(solidity::test::isValidMetadata(metadata_str));
		BOOST_CHECK(metadata.isMember("settings"));
		BOOST_CHECK(metadata["settings"].isMember("metadata"));
		BOOST_CHECK(metadata["settings"]["metadata"].isMember("bytecodeHash"));
		if (_literal)
		{
			BOOST_CHECK(metadata["settings"]["metadata"].isMember("useLiteralContent"));
			BOOST_CHECK(metadata["settings"]["metadata"]["useLiteralContent"].asBool());
		}
	};

	check(sourceCode, true);
	check(sourceCode, false);
}

BOOST_AUTO_TEST_CASE(metadata_viair)
{
	char const* sourceCode = R"(
		pragma solidity >=0.0;
		contract test {
		}
	)";

	auto check = [](char const* _src, bool _viair)
	{
		CompilerStack compilerStack;
		compilerStack.setSources({{"", std::string(_src)}});
		compilerStack.setEVMVersion(solidity::test::CommonOptions::get().evmVersion());
		compilerStack.setOptimiserSettings(solidity::test::CommonOptions::get().optimize);
		compilerStack.setViaIR(_viair);
		BOOST_REQUIRE_MESSAGE(compilerStack.compile(), "Compiling contract failed");
		string metadata_str = compilerStack.metadata("test");
		Json::Value metadata;
		util::jsonParseStrict(metadata_str, metadata);
		BOOST_CHECK(solidity::test::isValidMetadata(metadata_str));
		BOOST_CHECK(metadata.isMember("settings"));
		if (_viair)
		{
			BOOST_CHECK(metadata["settings"].isMember("viaIR"));
			BOOST_CHECK(metadata["settings"]["viaIR"].asBool());
		}
	};

	check(sourceCode, true);
	check(sourceCode, false);
}

BOOST_AUTO_TEST_CASE(metadata_revert_strings)
{
	CompilerStack compilerStack;
	char const* sourceCodeA = R"(
		pragma solidity >=0.0;
		contract A {
		}
	)";
	compilerStack.setSources({{"A", std::string(sourceCodeA)}});
	compilerStack.setRevertStringBehaviour(RevertStrings::Strip);
	BOOST_REQUIRE_MESSAGE(compilerStack.compile(), "Compiling contract failed");

	std::string const& serialisedMetadata = compilerStack.metadata("A");
	BOOST_CHECK(solidity::test::isValidMetadata(serialisedMetadata));
	Json::Value metadata;
	BOOST_REQUIRE(util::jsonParseStrict(serialisedMetadata, metadata));

	BOOST_CHECK_EQUAL(metadata["settings"]["debug"]["revertStrings"], "strip");
}

BOOST_AUTO_TEST_SUITE_END()

}
