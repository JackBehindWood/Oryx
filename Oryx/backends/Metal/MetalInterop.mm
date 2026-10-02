#include "oxpch.h"
#include "MetalInterop.h"

#include "Oryx/Core/Error.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <pthread.h>

@interface OryxMetalView : NSView
@end

@implementation OryxMetalView

- (CALayer*)makeBackingLayer
{
    return [CAMetalLayer layer];
}

- (void)syncLayer
{
    CAMetalLayer* layer = (CAMetalLayer*)self.layer;
    CGFloat scale = self.window != nil ? self.window.backingScaleFactor : 1.0;
    NSSize size = self.bounds.size;
    layer.contentsScale = scale;
    layer.drawableSize = CGSizeMake(size.width * scale, size.height * scale);
}

- (void)viewDidChangeBackingProperties
{
    [super viewDidChangeBackingProperties];
    [self syncLayer];
}

- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    [self syncLayer];
}

- (void)setFrameSize:(NSSize)size
{
    [super setFrameSize:size];
    [self syncLayer];
}

// Input belongs to GLFW's content view underneath.
- (NSView*)hitTest:(NSPoint)point
{
    return nil;
}

@end

namespace oryx::metal
{

namespace
{

void require_main_thread(const char* what)
{
    if (pthread_main_np() == 0)
    {
        throw Error(std::string(what) + " must run on the main thread");
    }
}

template<typename F>
void guarded(const char* what, F&& body)
{
    require_main_thread(what);
    std::string failure;
    @try
    {
        body();
    }
    @catch (NSException* exception)
    {
        failure = std::string(what) + ": " + exception.reason.UTF8String;
    }
    if (!failure.empty())
    {
        throw Error(failure);
    }
}

} // namespace

struct MetalSurface::Impl
{
    NSWindow* window = nil;
    OryxMetalView* view = nil;

    CAMetalLayer* layer() const { return (CAMetalLayer*)view.layer; }
};

MetalSurface::MetalSurface(void* ns_window, MTL::Device* device, const MetalSurfaceDesc& desc)
    : m_impl(create_unique<Impl>())
{
    if (ns_window == nullptr)
    {
        throw Error("MetalSurface requires a native window");
    }
    guarded("MetalSurface create", [&] {
        m_impl->window = (__bridge NSWindow*)ns_window;
        NSView* content = m_impl->window.contentView;
        OryxMetalView* view = [[OryxMetalView alloc] initWithFrame:content.bounds];
        view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        view.wantsLayer = YES;
        CAMetalLayer* layer = (CAMetalLayer*)view.layer;
        layer.device = (__bridge id<MTLDevice>)device;
        layer.displaySyncEnabled = desc.vsync;
        [content addSubview:view];
        m_impl->view = view;
        [view syncLayer];
    });
}

MetalSurface::~MetalSurface()
{
    if (m_impl == nullptr || m_impl->view == nil)
    {
        return;
    }
    try
    {
        guarded("MetalSurface destroy", [&] {
            [m_impl->view removeFromSuperview];
            m_impl->view = nil;
        });
    }
    catch (const Error& error)
    {
        OX_CORE_ERROR("{}", error.what());
    }
}

CA::MetalLayer* MetalSurface::layer() const
{
    return (__bridge CA::MetalLayer*)m_impl->layer();
}

uint32_t MetalSurface::width_px() const
{
    return static_cast<uint32_t>(m_impl->layer().drawableSize.width);
}

uint32_t MetalSurface::height_px() const
{
    return static_cast<uint32_t>(m_impl->layer().drawableSize.height);
}

float MetalSurface::content_scale() const
{
    return static_cast<float>(m_impl->layer().contentsScale);
}

bool MetalSurface::is_visible() const
{
    bool visible = false;
    guarded("MetalSurface visibility", [&] {
        NSWindow* window = m_impl->view.window;
        visible = window != nil && !window.miniaturized && (window.occlusionState & NSWindowOcclusionStateVisible) != 0;
    });
    return visible;
}

void MetalSurface::set_vsync(bool vsync)
{
    guarded("MetalSurface set_vsync", [&] { m_impl->layer().displaySyncEnabled = vsync; });
}

void MetalSurface::set_colourspace_srgb()
{
    guarded("MetalSurface set_colourspace_srgb", [&] {
        CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        m_impl->layer().colorspace = space;
        CGColorSpaceRelease(space);
    });
}

void MetalSurface::resize(uint32_t width_px, uint32_t height_px, float scale)
{
    guarded("MetalSurface resize", [&] {
        CAMetalLayer* layer = m_impl->layer();
        layer.contentsScale = scale;
        layer.drawableSize = CGSizeMake(width_px, height_px);
    });
}

} // namespace oryx::metal
