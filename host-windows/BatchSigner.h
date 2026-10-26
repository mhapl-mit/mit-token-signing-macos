/*
 * Chrome Token Signing Native Host
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <string>
#include <vector>

// SIGN_BATCH: signs all hashes with one PIN entry through the PKCS#11 module of the token
// vendor, like the macOS and Linux hosts. Tokens without a PKCS#11 module are signed one
// by one through the Windows certificate store, the same way as SIGN.
class BatchSigner {
public:
	static std::vector<std::vector<unsigned char>> sign(const std::vector<unsigned char> &cert,
		const std::vector<std::vector<unsigned char>> &digests, const std::string &info);
};
