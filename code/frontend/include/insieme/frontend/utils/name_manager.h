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

#pragma once

#include "insieme/frontend/clang.h"

namespace insieme {
namespace frontend {
namespace utils {

	using namespace llvm;

	/**
	 * Remove all symbols which are not allowed in a C identifier from the given string
	 */
	std::string removeSymbols(std::string str);

	/**
	 * Create a name for an anonymous object (encodes location)
	 */
	std::string createNameForAnon(const std::string& prefix, const clang::Decl* decl, const clang::SourceManager& sm);

	/**
	 * we build a complete name for the class,
	 * qualified name does not have the specific types of the spetialization
	 * the record provides que qualified name, the type the spetialization for the type
	 * we merge both strings in a safe string for the output
	 */
	std::string getNameForRecord(const clang::NamedDecl* decl, const clang::Type* type, const clang::SourceManager& sm);

	/**
	 * build a string to identify a function
	 * the produced string will be output-compatible, this means that we can use this name
	 * to name functions and they will not have qualification issues.
	 * @param funcDecl: the function decl to name
	 * @return unique string value
	 */
	std::string buildNameForFunction(const clang::FunctionDecl* funcDecl);

	std::string getNameForGlobal(const clang::VarDecl* varDecl, const clang::SourceManager& sm);

	/**
	 * Get name for enumeration, either from typedef or generated for anonymous
	 * @param tagType clang TagType pointer
	 * @return name for enumeration
	 */
	std::string getNameForEnum(const clang::EnumDecl* enumDecl, const clang::SourceManager& sm);

	/**
	 * Get name for field (named or anonymous)
	 * @param fieldDecl the field declaration given by clang
	 * @return name for the field
	 */
	std::string getNameForField(const clang::FieldDecl* fieldDecl, const clang::SourceManager& sm);

} // End utils namespace
} // End frontend namespace
} // End insieme namespace
