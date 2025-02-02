/**
 * Copyright (c) 2002-2013 Distributed and Parallel Systems Group,
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

#include "insieme/core/forward_decls.h"

namespace insieme {
namespace core {
namespace types {

	/**
	 * An exception type used if a return type could not successfully be deduced.
	 */
	class ReturnTypeDeductionException : public std::exception {
		// the message describing this type of exception
		const string msg;

	  public:
		ReturnTypeDeductionException(const string& msg = "Unable to deduce return type.") : msg(msg) {}
		virtual ~ReturnTypeDeductionException() throw() {}
		virtual const char* what() const throw() {
			return msg.c_str();
		}
	};

	/**
	 * This function is trying to deduce the type returned when calling a function having the
	 * given type by passing parameters of the given types. If the type cannot be deduced,
	 * an exception is raised.
	 *
	 * @param funType the type of the function to be invoked, for which the return type should be deduced
	 * @param argumentTypes the types of arguments passed to this function
	 * @return the deduced, most generic return type
	 * @throws ReturnTypeDeductionException if the requested type cannot be deduced
	 */
	TypePtr tryDeduceReturnType(const FunctionTypePtr& funType, const TypeList& argumentTypes);

	/**
	 * This function is deducing the type returned when calling a function having the given
	 * type by passing parameters of the given types.
	 *
	 * @param funType the type of the function to be invoked, for which the return type should be deduced
	 * @param argumentTypes the types of arguments passed to this function
	 * @return the deduced, most generic return type
	 */
	TypePtr deduceReturnType(const FunctionTypePtr& funType, const TypeList& argumentTypes, bool unitOnFail = true);


} // end namespace types
} // end namespace core
} // end namespace insieme
