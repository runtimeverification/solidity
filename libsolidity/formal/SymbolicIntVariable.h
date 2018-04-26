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

#pragma once

#include <libsolidity/formal/SymbolicVariable.h>

#include <libsolidity/ast/Types.h>

namespace dev
{
namespace solidity
{

/**
 * Specialization of SymbolicVariable for Integers
 */
class SymbolicIntVariable: public SymbolicVariable
{
public:
	SymbolicIntVariable(
		Declaration const& _decl,
		smt::SolverInterface& _interface
	);

	/// Sets the var to 0.
	void setZeroValue(int _seq);
	/// Sets the variable to the full valid value range.
	void setUnknownValue(int _seq);

	static smt::Expression minValue(IntegerType const& _t);
	static smt::Expression maxValue(IntegerType const& _t);

protected:
	smt::Expression valueAtSequence(int _seq) const;
};

}
}
