/*-
 * This file is part of Libight <https://libight.github.io/>.
 *
 * Libight is free software. See AUTHORS and LICENSE for more
 * information on the copying conditions.
 */

#ifndef LIBIGHT_COMMON_ERROR_H
# define LIBIGHT_COMMON_ERROR_H

namespace ight {
namespace common {
namespace error {

struct Error {
	int error = 0;

	Error(int e) {
		this->error = e;
	};
};

}}}  // namespaces
#endif  /* LIBIGHT_COMMON_ERROR_H */