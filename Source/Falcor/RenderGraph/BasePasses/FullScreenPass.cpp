/***************************************************************************
# Copyright (c) 2018, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************/
#include "stdafx.h"
#include "FullScreenPass.h"

namespace Falcor
{
    namespace
    {
        struct FullScreenPassData
        {
            Buffer::SharedPtr pVertexBuffer;
            Vao::SharedPtr pVao;
            uint64_t objectCount = 0;
        };

        FullScreenPassData gFullScreenData;

        bool checkForViewportArray2Support()
        {
#ifdef FALCOR_D3D12
            return false;
#elif defined FALCOR_VK
            return false;
#else
#error Unknown API
#endif
        }
        struct Vertex
        {
            glm::vec2 screenPos;
            glm::vec2 texCoord;
        };

#ifdef FALCOR_VK
#define ADJUST_Y(a) (-(a))
#else
#define ADJUST_Y(a) a
#endif

        const Vertex kVertices[] =
        {
            {glm::vec2(-1, ADJUST_Y(1)), glm::vec2(0, 0)},
            {glm::vec2(-1, ADJUST_Y(-1)), glm::vec2(0, 1)},
            {glm::vec2(1, ADJUST_Y(1)), glm::vec2(1, 0)},
            {glm::vec2(1, ADJUST_Y(-1)), glm::vec2(1, 1)},
        };
#undef ADJUST_Y

        void initFullScreenData(Buffer::SharedPtr& pVB, Vao::SharedPtr& pVao)
        {
            // First time we got here. create VB and VAO
            const uint32_t vbSize = (uint32_t)(sizeof(Vertex)*arraysize(kVertices));
            pVB = Buffer::create(vbSize, Buffer::BindFlags::Vertex, Buffer::CpuAccess::Write, (void*)kVertices);

            // create VAO
            VertexLayout::SharedPtr pLayout = VertexLayout::create();
            VertexBufferLayout::SharedPtr pBufLayout = VertexBufferLayout::create();
            pBufLayout->addElement("POSITION", 0, ResourceFormat::RG32Float, 1, 0);
            pBufLayout->addElement("TEXCOORD", 8, ResourceFormat::RG32Float, 1, 1);
            pLayout->addBufferLayout(0, pBufLayout);

            Vao::BufferVec buffers{ pVB };
            pVao = Vao::create(Vao::Topology::TriangleStrip, pLayout, buffers);

            if (!pVao || !pLayout || !pVB) throw std::exception();
        }
    }

    FullScreenPass::FullScreenPass(const Program::Desc& progDesc, const Program::DefineList& programDefines) : BaseGraphicsPass(progDesc, programDefines)
    {
        gFullScreenData.objectCount++;

        // create depth stencil state
        auto pDsState = DepthStencilState::create(DepthStencilState::Desc().setDepthEnabled(false));
        mpState->setDepthStencilState(pDsState);

        if (gFullScreenData.pVertexBuffer == nullptr) initFullScreenData(gFullScreenData.pVertexBuffer, gFullScreenData.pVao);
        mpState->setVao(gFullScreenData.pVao);

        if (!mpState || !gFullScreenData.pVao) throw std::exception();
    }

    FullScreenPass::~FullScreenPass()
    {
        assert(gFullScreenData.objectCount > 0);

        gFullScreenData.objectCount--;
        if (gFullScreenData.objectCount == 0)
        {
            gFullScreenData.pVao = nullptr;
            gFullScreenData.pVertexBuffer = nullptr;
        }
    }

    FullScreenPass::SharedPtr FullScreenPass::create(const Program::Desc& desc, const Program::DefineList& defines, uint32_t viewportMask)
    {
        Program::Desc d = desc;
        Program::DefineList defs = defines;
        std::string gs;

        if (viewportMask)
        {
            defs.add("_VIEWPORT_MASK", std::to_string(viewportMask));
            if (checkForViewportArray2Support())
            {
                defs.add("_USE_VP2_EXT");
            }
            else
            {
                defs.add("_OUTPUT_VERTEX_COUNT", std::to_string(3 * popcount(viewportMask)));
                d.addShaderLibrary("Framework/Shaders/FullScreenPass.gs.slang").gsEntry("main");
            }
        }
        if (d.getShaderEntryPoint(ShaderType::Vertex).empty()) d.addShaderLibrary("Framework/Shaders/FullScreenPass.vs.slang").vsEntry("main");

        try
        {
            return SharedPtr(new FullScreenPass(d, defs));
        }
        catch (std::exception) { return nullptr; }
    }

    FullScreenPass::SharedPtr FullScreenPass::create(const std::string& psFile, const Program::DefineList& defines, uint32_t viewportMask)
    {
        Program::Desc d;
        d.addShaderLibrary(psFile).psEntry("main");
        return create(d, defines, viewportMask);
    }

    void FullScreenPass::execute(RenderContext* pRenderContext, const Fbo::SharedPtr& pFbo, bool autoSetVpSc) const
    {
        mpState->setFbo(pFbo, autoSetVpSc);
        pRenderContext->draw(mpState.get(), mpVars.get(), arraysize(kVertices), 0);
    }
}