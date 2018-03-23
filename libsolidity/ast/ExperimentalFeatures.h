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
 * List of experimental features.
 */

#pragma once

#include <map>

namespace dev
{
namespace solidity
{

enum class ExperimentalFeature
{
	ABIEncoderV2, // new ABI encoder that makes use of JULIA
	SMTChecker,
	V050, // v0.5.0 breaking changes
	Test,
	TestOnlyAnalysis
};

static const std::map<ExperimentalFeature, bool> ExperimentalFeatureOnlyAnalysis =
{
	{ ExperimentalFeature::SMTChecker, true },
	{ ExperimentalFeature::TestOnlyAnalysis, true },
	{ ExperimentalFeature::V050, true }
};

static const std::map<std::string, ExperimentalFeature> ExperimentalFeatureNames =
{
	{ "ABIEncoderV2", ExperimentalFeature::ABIEncoderV2 },
	{ "SMTChecker", ExperimentalFeature::SMTChecker },
	{ "v0.5.0", ExperimentalFeature::V050 },
	{ "__test", ExperimentalFeature::Test },
	{ "__testOnlyAnalysis", ExperimentalFeature::TestOnlyAnalysis },
};

}
}
