/**
 * Copyright (c) 2002-2015 Distributed and Parallel Systems Group,
 *                Institute of Computer Science,
 *               University of Innsbruck, Austria
 *
 * This file is part of the INSIEME Compiler and Runtime System.
 *
 * We provide the software of this file (below described as "INSIEME")
 * under GPL Version 3.0 on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 *
 * If you require different license terms for your intended use of the
 * software, e.g. for proprietary commercial or industrial use, please
 * contact us at:
 *                   insieme@dps.uibk.ac.at
 *
 * We kindly ask you to acknowledge the use of this software in any
 * publication or other disclosure of results by referring to the
 * following citation:
 *
 * H. Jordan, P. Thoman, J. Durillo, S. Pellegrini, P. Gschwandtner,
 * T. Fahringer, H. Moritsch. A Multi-Objective Auto-Tuning Framework
 * for Parallel Codes, in Proc. of the Intl. Conference for High
 * Performance Computing, Networking, Storage and Analysis (SC 2012),
 * IEEE Computer Society Press, Nov. 2012, Salt Lake City, USA.
 *
 * All copyright notices must be kept intact.
 *
 * INSIEME depends on several third party software packages. Please
 * refer to http://www.dps.uibk.ac.at/insieme/license.html for details
 * regarding third party software licenses.
 */

#include "insieme/core/lang/extension.h"

#include "insieme/core/parser/ir_parser.h"
#include "insieme/core/ir_expressions.h"

#include "insieme/core/ir_node.h"
#include "insieme/core/ir_builder.h"
#include "insieme/utils/assert.h"

#include <string>
#include <map>

namespace insieme {
namespace core {
namespace lang {

	void Extension::checkIrNameNotAlreadyInUse(const string& irName) const {
		// only check for the existence of this name only if we define a new one
		if(irName.empty()) { return; }

		// first check the names defined in this extension here
		if(symbols.find(irName) != symbols.end()) {
			assert_fail() << "IR_NAME \"" << irName << "\" already in use in this extension";
		}

	}

	TypePtr Extension::getType(NodeManager& manager, const string& type, const symbol_map& definitions, const type_alias_map& aliases) {
		// build type
		TypePtr res = parser::parseType(manager, type, false, definitions, aliases);
		assert_true(res) << "Unable to parse type: " << type;
		return res;
	}

	LiteralPtr Extension::getLiteral(NodeManager& manager, const string& type, const string& value, const symbol_map& definitions, const type_alias_map& aliases) {
		return Literal::get(manager, getType(manager, type, definitions, aliases), value);
	}

	ExpressionPtr Extension::getExpression(NodeManager& manager, const string& spec, const symbol_map& definitions, const type_alias_map& aliases) {
		insieme::core::IRBuilder builder(manager);
		return builder.normalize(parser::parseExpr(manager, spec, false, definitions, aliases));
	}

} // end namespace lang
} // end namespace core
} // end namespace insieme
