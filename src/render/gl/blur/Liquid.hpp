#pragma once

#include "Kawase.hpp"

namespace Render::GL {
    // Liquid glass, after MyGlass (github.com/Sidharth7082/myglass, BSD-3-Clause): a thick glass slab over the blur.
    // The rim bends what is behind it inward with a per-channel spread, the middle swells a little like a dome, bright
    // backdrops are dimmed (and dark ones lifted) so text over the glass stays readable, and light catches the rim and
    // the top edge while the bottom edge falls into shadow. Every sample is taken inside the pane, so it reaches no
    // further than the blur under it.
    class CLiquidBlurMaterial final : public IGLBlurMaterial {
      public:
        eBlurType                 type() const noexcept override;
        SBlurMaterialRequirements requirements() const noexcept override;
        void                      bindFinish(CRenderContext& ctx, WP<CShader> shader, const SBlurMaterialContext& context) const override;
    };

    class CLiquidBlurProvider final : public CDualKawaseBlurProvider {
      public:
        explicit CLiquidBlurProvider(CHyprOpenGLImpl& impl);
    };
}
