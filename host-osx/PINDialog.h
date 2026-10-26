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

#import <Cocoa/Cocoa.h>

@interface PINPanel : NSObject

// Maximum number of hashes in one SIGN_BATCH request, reported in VERSION response
#define MAX_BATCH 100

// Signs "hash" (SIGN) or up to MAX_BATCH "hashes" with one PIN entry (SIGN_BATCH)
+ (NSDictionary *)show:(NSDictionary*)params cert:(NSString*)cert batch:(BOOL)batch;

@end
