#include "FrameBuffer.h"
#include "FrameBufferManager.h"
#include <arcana/macros.h>
#include <cmath>

namespace Babylon
{
    FrameBuffer::FrameBuffer(FrameBufferManager& manager, bgfx::FrameBufferHandle handle, uint16_t width, uint16_t height, bool defaultBackBuffer)
        : m_manager{manager}
        , m_handle{handle}
        , m_width{width}
        , m_height{height}
        , m_defaultBackBuffer{defaultBackBuffer}
        , m_viewId{}
        , m_viewPort{}
        , m_requestedViewPort{}
    {
    }

    FrameBuffer::~FrameBuffer()
    {
        if (bgfx::isValid(m_handle))
        {
            bgfx::destroy(m_handle);
        }
    }

    bgfx::FrameBufferHandle FrameBuffer::Handle() const
    {
        return m_handle;
    }

    uint16_t FrameBuffer::Width() const
    {
        return (m_width == 0 ? bgfx::getStats()->width : m_width);
    }

    uint16_t FrameBuffer::Height() const
    {
        return (m_height == 0 ? bgfx::getStats()->height : m_height);
    }

    bool FrameBuffer::DefaultBackBuffer() const
    {
        return m_defaultBackBuffer;
    }

    void FrameBuffer::Clear(bgfx::Encoder* encoder, uint16_t flags, uint32_t rgba, float depth, uint8_t stencil)
    {
        NewView<true>(encoder, {});

        bgfx::setViewClear(m_viewId.value(), flags, rgba, depth, stencil);
    }

    void FrameBuffer::SetViewPort(bgfx::Encoder* encoder, float x, float y, float width, float height)
    {
        // We want to make sure the caller has gotten an encoder through the update token
        // even though we don't actuall use the encoder here directly.
        UNUSED(encoder);
        m_requestedViewPort = {x, y, width, height};
    }

    void FrameBuffer::Submit(bgfx::Encoder* encoder, bgfx::ProgramHandle programHandle, uint8_t flags)
    {
        if (m_requestedViewPort.has_value() && !m_requestedViewPort->Equals(m_viewPort))
        {
            NewView<false>(encoder, m_requestedViewPort.value());
        }
        else if (!m_viewId.has_value())
        {
            NewView<false>(encoder, {});
        }

        encoder->submit(m_viewId.value(), programHandle, 0, flags);
    }

    void FrameBuffer::Blit(bgfx::Encoder* encoder, bgfx::TextureHandle dst, uint16_t dstX, uint16_t dstY, bgfx::TextureHandle src, uint16_t srcX, uint16_t srcY, uint16_t width, uint16_t height)
    {
        // 1 blit per view, create a new viewId for each blit
        NewView<false>(encoder, {});
        encoder->blit(m_viewId.value(), dst, dstX, dstY, src, srcX, srcY, width, height);
    }

    void FrameBuffer::SetStencil(bgfx::Encoder* encoder, uint32_t stencilState)
    {
        encoder->setStencil(stencilState);
    }

    void FrameBuffer::Reset()
    {
        m_viewId.reset();
        m_viewPort = {};
        m_requestedViewPort.reset();
    }

    bool FrameBuffer::ViewPort::Equals(const ViewPort& other) const
    {
        return
            std::abs(X - other.X) < std::numeric_limits<float>::epsilon() &&
            std::abs(Y - other.Y) < std::numeric_limits<float>::epsilon() &&
            std::abs(Width - other.Width) < std::numeric_limits<float>::epsilon() &&
            std::abs(Height - other.Height) < std::numeric_limits<float>::epsilon();
    }

    template<bool doTouch> void FrameBuffer::NewView(bgfx::Encoder* encoder, const ViewPort& viewPort)
    {
        m_viewId = m_manager.NewViewId();
        m_viewPort = viewPort;

        bgfx::setViewMode(m_viewId.value(), bgfx::ViewMode::Sequential);
        bgfx::setViewClear(m_viewId.value(), BGFX_CLEAR_NONE);
        bgfx::setViewFrameBuffer(m_viewId.value(), m_handle);
        bgfx::setViewRect(m_viewId.value(),
            static_cast<uint16_t>(m_viewPort.X * Width()),
            static_cast<uint16_t>(m_viewPort.Y * Height()),
            static_cast<uint16_t>(m_viewPort.Width * Width()),
            static_cast<uint16_t>(m_viewPort.Height * Height()));

        // This dummy draw call is here to make sure that the view is cleared
        // if no other draw calls are submitted to the view.
        if (doTouch)
        {
            encoder->touch(m_viewId.value());
        }
    }

    void FrameBuffer::AcquireNewViewId()
    {
        NewView<false>(nullptr, m_viewPort);
    }
}
