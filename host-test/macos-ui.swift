// Drives the dialogs of the native host for macos-ui-test.sh. The host is an agent app (LSUIElement)
// started from its executable, System Events sees no windows of it, so this works with CoreGraphics:
//
//   macos-ui window <pid>      prints "x y width height" of the front window of the process
//   macos-ui click <x> <y>     clicks at the point
//   macos-ui type <text>       types the text
//   macos-ui return            presses Return
import CoreGraphics
import Foundation

func post(_ event: CGEvent?) {
    event?.post(tap: .cghidEventTap)
    usleep(50_000)
}

func key(_ code: CGKeyCode, _ text: String? = nil) {
    for down in [true, false] {
        let event = CGEvent(keyboardEventSource: nil, virtualKey: code, keyDown: down)
        if let text = text {
            let chars = Array(text.utf16)
            event?.keyboardSetUnicodeString(stringLength: chars.count, unicodeString: chars)
        }
        post(event)
    }
}

let args = CommandLine.arguments
switch args.count > 1 ? args[1] : "" {
case "window":
    let pid = Int32(args[2])!
    let windows = CGWindowListCopyWindowInfo([.optionOnScreenOnly, .excludeDesktopElements], kCGNullWindowID) as? [[String: Any]] ?? []
    for window in windows where (window[kCGWindowOwnerPID as String] as? Int32) == pid
            && (window[kCGWindowLayer as String] as? Int) != nil {
        if let bounds = window[kCGWindowBounds as String] as? [String: CGFloat],
           let width = bounds["Width"], let height = bounds["Height"], width > 100, height > 50 {
            print(Int(bounds["X"]!), Int(bounds["Y"]!), Int(width), Int(height))
            exit(0)
        }
    }
    exit(1)
case "click":
    let point = CGPoint(x: Double(args[2])!, y: Double(args[3])!)
    post(CGEvent(mouseEventSource: nil, mouseType: .leftMouseDown, mouseCursorPosition: point, mouseButton: .left))
    post(CGEvent(mouseEventSource: nil, mouseType: .leftMouseUp, mouseCursorPosition: point, mouseButton: .left))
case "type":
    for char in args[2] {
        key(0, String(char))
    }
case "return":
    key(36)
default:
    print("usage: macos-ui window <pid> | click <x> <y> | type <text> | return")
    exit(2)
}
