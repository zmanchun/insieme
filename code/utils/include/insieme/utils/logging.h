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

#include <iostream>
#include <mutex>

#include "insieme/utils/abstraction.h"

namespace insieme {
namespace utils {
namespace log {

	/**
	 * An enumeration of the supported log levels.
	 * Each level includes all the messages of the higher levels.
	 * E.g. all errors are also printed in case the mode is set to warning.
	 */
	enum Level { DEBUG, INFO, WARNING, ERROR, FATAL };

	/**
	 * The name of the environment variable to set up the log level.
	 */
	#define LOG_LEVEL_ENV "INSIEME_LOG_LEVEL"

	/**
	 * The name of the environment variable to set up the verbosity level.
	 */
	#define LOG_VERBOSITY_ENV "INSIEME_LOG_VERBOSITY"

	/**
	 * The name of the environment variable to set up a regular expression filtering
	 * log messages by function names.
	 */
	#define LOG_FILTER_ENV "INSIEME_LOG_FILTER"

} // end namespace log

/**
 * Another namespace to avoid the import of those symbols
 * into other namespaces.
 */
namespace logger_details {

	/**
	 * A globally visible variable determining the logging level.
	 */
	extern log::Level g_level;

	/**
	 * Temporary object used to wrap the log stream. This object is responsible to
	 * collect logs and flush the stream once the object is deallocated.
	 *
	 * A lock is used to maintain exclusivity of logs, in case of multi-threaded application
	 * the logger guarantees mutual exclusion between threads using the stream.
	 */
	class SynchronizedStream {

		// the underlying stream
		std::ostream& out;

		// a reference to a synchronizing mutex
		std::mutex& mutex;

		// a flag indicating that this instance is the current owner of the stream
		bool owner;

	public:

		SynchronizedStream(std::ostream& out, std::mutex& mutex) : out(out), mutex(mutex), owner(true) {
			mutex.lock();		// acquire exclusive access to the output stream
		}

		SynchronizedStream(SynchronizedStream&& other) : out(other.out), mutex(other.mutex), owner(true) {
			other.owner = false;  // pass on access to output stream
		}

		~SynchronizedStream() {
			if (owner) {
				out << std::endl;
				out.flush();
				mutex.unlock();	// release access to output stream
			}
		}

		std::ostream& getStream() const {
			return out;
		}
	};

	/**
	 * Obtains a properly set up stream for the printing of a message of the given
	 * level, in the given file and line.
	 */
	SynchronizedStream getLogStreamFor(log::Level level, const char* file, int line);

	/**
	 * Determines whether the given function name is covered by the current function
	 * name filter setup.
	 */
	bool isIncludedInFilter(const char* fullFunctionName);

	/**
	 * Determines the current verbosity level.
	 */
	unsigned short getVerbosityLevel();

	/**
	 * Causes the logging system to re-load its configuration from the environment
	 * variables.
	 */
	void reloadConfiguration();

} // end namespace logger_details
} // end namespace utils
} // end namespace insieme

/**
 * Import logging namespace into all translation units to access
 * LOG-Level enumeration without qualification.
 */
using namespace insieme::utils::log;

#define LOG(LEVEL)                                                                                                                                             \
	if(insieme::utils::logger_details::g_level > LEVEL || !insieme::utils::logger_details::isIncludedInFilter(FUNCTION_SIGNATURE)) {/* nothing */               \
	} else                                                                                                                                                     \
	insieme::utils::logger_details::getLogStreamFor(LEVEL, __FILE__, __LINE__).getStream()

#define VLOG(VerbLevel)                                                                                                                                        \
	if(VerbLevel > insieme::utils::logger_details::getVerbosityLevel()) {                                                                                      \
	} else                                                                                                                                                     \
	LOG(DEBUG)

#define VLOG_IS_ON(VerbLevel)                                                                                                                                  \
	(VerbLevel <= insieme::utils::logger_details::getVerbosityLevel() && insieme::utils::logger_details::isIncludedInFilter(FUNCTION_SIGNATURE))
