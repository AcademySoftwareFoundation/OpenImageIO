#import <Cocoa/Cocoa.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#include "dnd_glfw.h"

using dnd_glfw::PayloadKind;
using dnd_glfw::detail::WindowState;
using dnd_glfw::detail::dispatchDragEnter;
using dnd_glfw::detail::dispatchDragOver;
using dnd_glfw::detail::dispatchDragLeave;
using dnd_glfw::detail::dispatchDragCancel;
using dnd_glfw::detail::dispatchDrop;

@interface DngGlfwDragView : NSView <NSDraggingDestination>
{
@public
    WindowState* dngState;
}
@end

@implementation DngGlfwDragView

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
    if (!dngState) {
        return NSDragOperationNone;
    }

    NSPasteboard* pasteboard = [sender draggingPasteboard];
    if (!pasteboard) {
        return NSDragOperationNone;
    }

    NSDictionary* options = @{ NSPasteboardURLReadingFileURLsOnlyKey : @YES };
    NSArray* urls         = [pasteboard readObjectsForClasses:@[ [NSURL class] ] options:options];
    if (!urls || [urls count] == 0) {
        return NSDragOperationNone;
    }

    dispatchDragEnter(dngState, 0.0, 0.0, PayloadKind::Files);
    return NSDragOperationCopy;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender
{
    (void)sender;

    if (!dngState) {
        return NSDragOperationNone;
    }

    dispatchDragOver(dngState, 0.0, 0.0, PayloadKind::Files);
    return NSDragOperationCopy;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender
{
    (void)sender;

    if (!dngState) {
        return;
    }

    dispatchDragLeave(dngState);
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
    if (!dngState) {
        return NO;
    }

    NSPasteboard* pasteboard = [sender draggingPasteboard];
    if (!pasteboard) {
        dispatchDragCancel(dngState);
        return NO;
    }

    NSDictionary* options = @{ NSPasteboardURLReadingFileURLsOnlyKey : @YES };
    NSArray* urls         = [pasteboard readObjectsForClasses:@[ [NSURL class] ] options:options];
    if (!urls || [urls count] == 0) {
        dispatchDragCancel(dngState);
        return NO;
    }

    dnd_glfw::DropEvent ev;
    ev.kind = PayloadKind::Files;

    NSWindow* nsWindow = [self window];
    if (nsWindow) {
        NSView* contentView = [nsWindow contentView];
        if (contentView) {
            const NSRect contentRect = [contentView frame];
            const NSPoint pos        = [sender draggingLocation];
            ev.x                     = pos.x;
            ev.y                     = contentRect.size.height - pos.y;
        }
    }

    for (NSURL* url in urls) {
        if (![url isFileURL]) {
            continue;
        }
        const char* cpath = [url fileSystemRepresentation];
        if (cpath) {
            ev.paths.emplace_back(cpath);
        }
    }

    if (ev.paths.empty()) {
        dispatchDragCancel(dngState);
        return NO;
    }

    dispatchDrop(dngState, ev);
    dispatchDragLeave(dngState);
    return YES;
}

@end

namespace dnd_glfw {
namespace detail {

void
platformInitMac(WindowState* state, GLFWwindow* window)
{
    if (!state || !window) {
        return;
    }

    id cocoaWindowObj = glfwGetCocoaWindow(window);
    NSWindow* nsWindow = (NSWindow*)cocoaWindowObj;
    if (!nsWindow) {
        return;
    }

    NSView* contentView = [nsWindow contentView];
    if (!contentView) {
        return;
    }

    DngGlfwDragView* dragView = [[DngGlfwDragView alloc] initWithFrame:[contentView bounds]];
    dragView->dngState        = state;
    [dragView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];

    [contentView addSubview:dragView positioned:NSWindowAbove relativeTo:nil];
    [dragView registerForDraggedTypes:@[ NSPasteboardTypeFileURL, NSFilenamesPboardType ]];

    state->cocoaView = (__bridge_retained void*)dragView;
}

void
platformShutdownMac(WindowState* state, GLFWwindow* window)
{
    (void)window;

    if (!state || !state->cocoaView) {
        return;
    }

    NSView* view = (__bridge_transfer NSView*)state->cocoaView;
    [view removeFromSuperview];
    state->cocoaView = nullptr;
}

} // namespace detail
} // namespace dnd_glfw
