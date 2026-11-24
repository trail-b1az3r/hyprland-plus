#include "Liquid.hpp"

#include "../../Renderer.hpp"
#include "../../Shader.hpp"
#include "../../ShaderLoader.hpp"
#include "../../../config/ConfigValue.hpp"
#include "../../../helpers/Color.hpp"
#include "../../../helpers/cm/ColorManagement.hpp"

#include <algorithm>

using namespace Render;
using namespace Render::GL;
using namespace NColorManagement;

// How far 1.0 of SDR white is from the work buffer's peak, so the highlights are the same brightness in HDR.
static float liquidLuminanceScale(CRenderContext& ctx) {
    const auto INTERMEDIATE = getDefaultImageDescription();
    const auto WORKBUFFER   = g_pHyprRenderer->workBufferImageDescription(ctx);
    if (!WORKBUFFER)
        return 1.F;

    const auto MINIMUM = INTERMEDIATE->value().getTFMinLuminance();
    const auto MAXIMUM = INTERMEDIATE->value().getTFMaxLuminance();
    const auto RANGE   = std::max(MAXIMUM, sc<float>(WORKBUFFER->value().luminances.max)) - MINIMUM;
    return (MAXIMUM - MINIMUM) / std::max(RANGE, 0.001F);
}

CLiquidBlurProvider::CLiquidBlurProvider(CHyprOpenGLImpl& impl) : CDualKawaseBlurProvider(impl, makeUnique<CLiquidBlurMaterial>()) {
    ;
}

eBlurType CLiquidBlurMaterial::type() const noexcept {
    return eBlurType::BLUR_LIQUID;
}

SBlurMaterialRequirements CLiquidBlurMaterial::requirements() const noexcept {
    return {
        .finishFragment = SH_FRAG_LIQUIDFINISH,
    };
}

void CLiquidBlurMaterial::bindFinish(CRenderContext& ctx, WP<CShader> shader, const SBlurMaterialContext& context) const {
    shader->setUniformInt(SHADER_LIQUID_ENABLED, 0);

    // The glass needs the pane's shape: without one (a blurred region that is not a window or a layer) it is plain blur.
    if (!context.blurContext.shape)
        return;

    const auto extent = context.blurContext.shape->box;
    if (extent.width <= 0 || extent.height <= 0)
        return;

    static auto PREFRACTION = CConfigValue<Config::FLOAT>("decoration:blur:liquid:refraction");
    static auto PABERRATION = CConfigValue<Config::FLOAT>("decoration:blur:liquid:aberration");
    static auto PEDGE       = CConfigValue<Config::FLOAT>("decoration:blur:liquid:edge");
    static auto PLENS       = CConfigValue<Config::FLOAT>("decoration:blur:liquid:lens");
    static auto PFRESNEL    = CConfigValue<Config::FLOAT>("decoration:blur:liquid:fresnel");
    static auto PSPECULAR   = CConfigValue<Config::FLOAT>("decoration:blur:liquid:specular");
    static auto PDIM        = CConfigValue<Config::FLOAT>("decoration:blur:liquid:adaptive_dim");
    static auto PBOOST      = CConfigValue<Config::FLOAT>("decoration:blur:liquid:adaptive_boost");
    static auto PSATURATION = CConfigValue<Config::FLOAT>("decoration:blur:liquid:saturation");
    static auto PTINT       = CConfigValue<Config::INTEGER>("decoration:blur:liquid:tint");

    const auto  TINT = CHyprColor(*PTINT);

    shader->setUniformInt(SHADER_LIQUID_ENABLED, 1);
    shader->setUniformFloat4(SHADER_LIQUID_EXTENT, sc<float>(extent.x), sc<float>(extent.y), sc<float>(extent.width), sc<float>(extent.height));
    shader->setUniformFloat(SHADER_LIQUID_RADIUS, std::max(context.blurContext.shape->radius, 0.F));
    shader->setUniformFloat(SHADER_LIQUID_ROUNDING_POWER, std::max(context.blurContext.shape->roundingPower, 1.F));
    shader->setUniformFloat(SHADER_LIQUID_REFRACTION, std::clamp(*PREFRACTION, 0.F, 64.F));
    shader->setUniformFloat(SHADER_LIQUID_ABERRATION, std::clamp(*PABERRATION, 0.F, 1.F));
    shader->setUniformFloat(SHADER_LIQUID_EDGE, std::clamp(*PEDGE, 0.01F, 0.5F));
    shader->setUniformFloat(SHADER_LIQUID_LENS, std::clamp(*PLENS, 0.F, 2.F));
    shader->setUniformFloat(SHADER_LIQUID_FRESNEL, std::clamp(*PFRESNEL, 0.F, 2.F));
    shader->setUniformFloat(SHADER_LIQUID_SPECULAR, std::clamp(*PSPECULAR, 0.F, 2.F));
    shader->setUniformFloat(SHADER_LIQUID_ADAPTIVE_DIM, std::clamp(*PDIM, 0.F, 1.F));
    shader->setUniformFloat(SHADER_LIQUID_ADAPTIVE_BOOST, std::clamp(*PBOOST, 0.F, 1.F));
    shader->setUniformFloat(SHADER_LIQUID_SATURATION, std::clamp(*PSATURATION, 0.F, 2.F));
    shader->setUniformFloat4(SHADER_LIQUID_TINT, sc<float>(TINT.r), sc<float>(TINT.g), sc<float>(TINT.b), std::clamp(sc<float>(TINT.a), 0.F, 1.F));
    shader->setUniformFloat(SHADER_LIQUID_STRENGTH, std::clamp(context.strength, 0.F, 1.F));
    shader->setUniformInt(SHADER_LIQUID_TRANSFER_FUNCTION, sc<int>(getDefaultImageDescription()->value().transferFunction));
    shader->setUniformFloat(SHADER_LIQUID_LUMINANCE_SCALE, liquidLuminanceScale(ctx));
}
