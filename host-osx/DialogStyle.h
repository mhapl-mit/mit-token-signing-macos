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

#import <Cocoa/Cocoa.h>

#include "Logo.h"

// Common look of the dialogs, the same as on Linux (host-linux/DialogStyle.h) and Windows
// (host-windows/BatchPinDialog): heading with the MIT logo on the text background colour,
// content over the full width, buttons in a footer below a separator. The controls come from
// the xib, they are moved into a new content view, so outlets and actions stay connected.
// Hidden rows take no space. System colours keep dark mode working.

static const CGFloat MIT_DIALOG_MARGIN = 24;

// The logo with its empty border cropped, the icon has a wide one
static NSImage *MITLogo(void)
{
    static NSImage *logo = [] {
        NSData *data = [NSData dataWithBytesNoCopy:(void *)MIT_LOGO_PNG length:sizeof(MIT_LOGO_PNG) freeWhenDone:NO];
        NSBitmapImageRep *image = [NSBitmapImageRep imageRepWithData:data];
        NSInteger left = image.pixelsWide, top = image.pixelsHigh, right = -1, bottom = -1;
        for (NSInteger y = 0; y < image.pixelsHigh; ++y) {
            for (NSInteger x = 0; x < image.pixelsWide; ++x) {
                NSColor *color = [[image colorAtX:x y:y] colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
                CGFloat darkest = MIN(MIN(color.redComponent, color.greenComponent), color.blueComponent);
                if (color.alphaComponent > 0.125 && darkest < 0.88) {
                    left = MIN(left, x);
                    right = MAX(right, x);
                    top = MIN(top, y);
                    bottom = MAX(bottom, y);
                }
            }
        }
        if (right < left) {
            return [[NSImage alloc] initWithData:data];
        }
        CGImageRef cropped = CGImageCreateWithImageInRect(image.CGImage, CGRectMake(left, top, right - left + 1, bottom - top + 1));
        NSImage *result = [[NSImage alloc] initWithCGImage:cropped size:NSZeroSize];
        CGImageRelease(cropped);
        return result;
    }();
    return logo;
}

static void MITSecondaryText(NSTextField *label)
{
    label.textColor = NSColor.secondaryLabelColor;
}

static void MITErrorText(NSTextField *label)
{
    label.textColor = NSColor.systemRedColor;
    label.font = [NSFont boldSystemFontOfSize:NSFont.systemFontSize];
}

// Rebuilds the content of the window. rows are full width from top to bottom, buttons are in
// the footer from left to right. Returns the heading label.
static NSTextField *MITStyleDialog(NSWindow *window, NSArray<NSView *> *rows, NSArray<NSButton *> *buttons, CGFloat width)
{
    window.title = @"MIT Token signing";
    const CGFloat contentWidth = width - 2 * MIT_DIALOG_MARGIN;

    NSTextField *heading = [NSTextField labelWithString:@""];
    heading.font = [NSFont systemFontOfSize:19 weight:NSFontWeightSemibold];
    heading.lineBreakMode = NSLineBreakByTruncatingTail;
    [heading setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow forOrientation:NSLayoutConstraintOrientationHorizontal];
    NSImageView *logo = [NSImageView imageViewWithImage:MITLogo()];
    logo.imageScaling = NSImageScaleProportionallyUpOrDown;
    NSSize logoSize = logo.image.size;
    [logo.heightAnchor constraintEqualToConstant:36].active = YES;
    [logo.widthAnchor constraintEqualToConstant:36 * logoSize.width / MAX(logoSize.height, 1)].active = YES;
    NSStackView *header = [[NSStackView alloc] init];
    header.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    [header addView:heading inGravity:NSStackViewGravityLeading];
    [header addView:logo inGravity:NSStackViewGravityTrailing];
    header.alignment = NSLayoutAttributeCenterY;
    header.spacing = 16;

    NSStackView *body = [NSStackView stackViewWithViews:@[header]];
    body.orientation = NSUserInterfaceLayoutOrientationVertical;
    body.alignment = NSLayoutAttributeLeading;
    body.spacing = 6;
    body.edgeInsets = NSEdgeInsetsMake(20, MIT_DIALOG_MARGIN, MIT_DIALOG_MARGIN, MIT_DIALOG_MARGIN);
    [body setCustomSpacing:14 afterView:header];
    [header.widthAnchor constraintEqualToConstant:contentWidth].active = YES;
    for (NSView *row in rows) {
        [row removeFromSuperview];
        row.translatesAutoresizingMaskIntoConstraints = NO;
        // The fixed width from the xib would fight the full width
        for (NSLayoutConstraint *constraint in row.constraints) {
            if (constraint.firstItem == row && constraint.secondItem == nil && constraint.firstAttribute == NSLayoutAttributeWidth) {
                constraint.active = NO;
            }
        }
        if ([row isKindOfClass:NSTextField.class] && !((NSTextField *)row).editable) {
            NSTextField *label = (NSTextField *)row;
            label.cell.scrollable = NO;
            label.cell.wraps = YES;
            label.lineBreakMode = NSLineBreakByWordWrapping;
            label.preferredMaxLayoutWidth = contentWidth;
        }
        [body addView:row inGravity:NSStackViewGravityTop];
        [row.widthAnchor constraintEqualToConstant:contentWidth].active = YES;
    }
    NSBox *separator = [[NSBox alloc] init];
    separator.boxType = NSBoxSeparator;

    NSStackView *footer = [[NSStackView alloc] init];
    footer.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    footer.spacing = 12;
    footer.edgeInsets = NSEdgeInsetsMake(14, MIT_DIALOG_MARGIN, 14, MIT_DIALOG_MARGIN);
    for (NSButton *button in buttons) {
        [button removeFromSuperview];
        button.translatesAutoresizingMaskIntoConstraints = NO;
        [button.widthAnchor constraintGreaterThanOrEqualToConstant:88].active = YES;
        [footer addView:button inGravity:NSStackViewGravityTrailing];
    }

    NSStackView *root = [NSStackView stackViewWithViews:buttons.count ? @[body, separator, footer] : @[body]];
    root.orientation = NSUserInterfaceLayoutOrientationVertical;
    root.alignment = NSLayoutAttributeWidth;
    root.spacing = 0;
    [root.widthAnchor constraintEqualToConstant:width].active = YES;
    // Background of the content behind it, a box takes no size from its content view
    NSBox *background = [[NSBox alloc] init];
    background.boxType = NSBoxCustom;
    background.borderWidth = 0;
    background.fillColor = NSColor.textBackgroundColor;
    background.translatesAutoresizingMaskIntoConstraints = NO;
    [root addSubview:background positioned:NSWindowBelow relativeTo:body];
    [NSLayoutConstraint activateConstraints:@[
        [background.leadingAnchor constraintEqualToAnchor:body.leadingAnchor],
        [background.trailingAnchor constraintEqualToAnchor:body.trailingAnchor],
        [background.topAnchor constraintEqualToAnchor:body.topAnchor],
        [background.bottomAnchor constraintEqualToAnchor:body.bottomAnchor],
    ]];
    window.contentView = root;
    [root layoutSubtreeIfNeeded];
    [window setContentSize:root.fittingSize];
    [window center];
    return heading;
}

// Call after the texts and visibility of the rows are set: the last visible row before gapBefore
// gets a larger gap, like above the PIN field label on the other platforms, and the window
// follows the new size, hidden rows take no space
static void MITFitDialog(NSWindow *window, NSArray<NSView *> *rows, NSView *gapBefore)
{
    NSStackView *body = (NSStackView *)rows.firstObject.superview;
    NSView *previous = nil;
    for (NSView *row in rows) {
        if (row == gapBefore) {
            break;
        }
        [body setCustomSpacing:body.spacing afterView:row];
        if (!row.hidden) {
            previous = row;
        }
    }
    if (previous && gapBefore) {
        [body setCustomSpacing:14 afterView:previous];
    }
    [window.contentView layoutSubtreeIfNeeded];
    [window setContentSize:window.contentView.fittingSize];
    [window center];
}
