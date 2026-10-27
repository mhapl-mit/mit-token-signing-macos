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

struct BatchPinRequest {
	std::wstring title;    // window title
	std::wstring heading;  // main instruction
	std::wstring name;     // certificate holder
	std::wstring details;  // number of documents or explanation
	std::wstring error;    // wrong PIN and tries left
	std::wstring label;    // PIN field
	std::wstring label2;   // digital signature PIN field, empty for a dialog with one field
	std::wstring sameLabel; // box that the digital signature PIN is the token PIN, disables label2
	bool same = true;      // initial state of the box
	bool signaturePin = false; // digital signature PIN dialog, details have room for three lines
	unsigned long minPinLen = 4;
};

// PIN dialog of batch signing in the style of current Windows dialogs, SIGN keeps using PinDialog
class BatchPinDialog {
public:
	// Returns the PIN as UTF-8, empty when cancelled. signaturePin gets the second field.
	// same gets the state of the box of sameLabel, signaturePin stays empty when it is checked.
	static std::string show(const BatchPinRequest &request, std::string *signaturePin = nullptr, bool *same = nullptr);
};
