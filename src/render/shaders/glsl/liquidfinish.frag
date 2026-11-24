#version 300 es
#define ALLOW_INCLUDES
#extension GL_ARB_shading_language_include : enable

// Liquid glass, after MyGlass (github.com/Sidharth7082/myglass, BSD-3-Clause, Copyright (c) 2025, Jeremy Trufier):
// its thick-glass model over Hyprland's blur. The tone steps (dimming, lift, tint, rim light) are done on
// gamma-encoded values, as MyGlass tuned them, and the result goes back to the work buffer's light.

precision highp float;

in vec2 v_texcoord;
uniform sampler2D tex;

uniform int liquidEnabled;
uniform vec4 liquidExtent;
uniform float liquidRadius;
uniform float liquidRoundingPower;
uniform float liquidRefraction;
uniform float liquidAberration;
uniform float liquidEdge;
uniform float liquidLens;
uniform float liquidFresnel;
uniform float liquidSpecular;
uniform float liquidAdaptiveDim;
uniform float liquidAdaptiveBoost;
uniform float liquidSaturation;
uniform vec4 liquidTint;
uniform float liquidStrength;
uniform int liquidTransferFunction;
uniform float liquidLuminanceScale;
uniform float noise;
uniform float brightness;

#include "defines.h"
#if USE_CM
uniform int sourceTF;
uniform int targetTF;
#include "CM.glsl"
#endif

#include "cm_helpers.glsl"
#include "blurFinish.glsl"

layout(location = 0) out vec4 fragColor;

const vec3 LIQUID_LUMA = vec3(0.2126, 0.7152, 0.0722);

float liquidLength(vec2 value, float power) {
    value = abs(value);
    return pow(pow(value.x, power) + pow(value.y, power), 1.0 / power);
}

float liquidBoxSDF(vec2 position, vec2 halfSize, float radius, float power) {
    radius = clamp(radius, 0.0, min(halfSize.x, halfSize.y));
    vec2 offset = abs(position) - (halfSize - vec2(radius));
    vec2 outside = max(offset, vec2(0.0));
    float corner = (outside.x > 0.0 || outside.y > 0.0) ? liquidLength(outside, power) : 0.0;
    return corner + min(max(offset.x, offset.y), 0.0) - radius;
}

vec3 liquidSample(vec2 uv) {
    vec4 color = texture(tex, uv);
    return toLinearRGB(color.rgb / max(color.a, 0.001), liquidTransferFunction);
}

vec4 applyLiquid(vec4 blurred) {
    float effect = clamp(liquidStrength, 0.0, 1.0);
    if (effect <= 0.0 || blurred.a <= 0.001)
        return blurred;

    vec2 halfSize = max(liquidExtent.zw * 0.5, vec2(1.0));
    vec2 position = gl_FragCoord.xy - (liquidExtent.xy + halfSize);
    float power = clamp(liquidRoundingPower, 1.0, 10.0);
    float sdf = liquidBoxSDF(position, halfSize, liquidRadius, power);
    float antialias = max(fwidth(sdf), 0.75);
    float shape = 1.0 - smoothstep(-antialias, antialias, sdf);
    vec2 texcoordDx = dFdx(v_texcoord);
    vec2 texcoordDy = dFdy(v_texcoord);
    if (shape <= 0.0)
        return blurred;

    // 1 at the rim, falling off inward over the bezel.
    float span = 2.0 * min(halfSize.x, halfSize.y);
    float bezel = max(liquidEdge * span, 1.0);
    float edge = exp(min(sdf, 0.0) / bezel);

    // The rim bends the backdrop toward the middle (toward the centre is smooth everywhere, corners included).
    float distanceToCentre = length(position);
    vec2 inward = distanceToCentre > 0.1 ? -position / distanceToCentre : vec2(0.0);
    vec2 offsetPixels = inward * edge * max(liquidRefraction, 0.0) * effect;

    // The middle swells a little, like a dome, fading out toward the rim.
    if (liquidLens > 0.001) {
        vec2 c = clamp(position / halfSize, vec2(-1.0), vec2(1.0));
        vec2 dome = vec2(-4.0 * c.x * (1.0 - c.y * c.y), -4.0 * c.y * (1.0 - c.x * c.x));
        offsetPixels += dome * liquidLens * span * 0.006 * (1.0 - edge) * effect;
    }

    vec2 offsetUV = offsetPixels.x * texcoordDx + offsetPixels.y * texcoordDy;
    vec2 halfTexel = 0.5 / vec2(textureSize(tex, 0));
    vec2 maximumUV = vec2(1.0) - halfTexel;

    // Blue bends more than red: coloured fringes at the rim.
    float spread = clamp(liquidAberration, 0.0, 1.0) * 0.35;
    vec3 colour;
    if (spread > 0.001 && edge > 0.01) {
        colour.r = liquidSample(clamp(v_texcoord + offsetUV * (1.0 - spread), halfTexel, maximumUV)).r;
        colour.g = liquidSample(clamp(v_texcoord + offsetUV, halfTexel, maximumUV)).g;
        colour.b = liquidSample(clamp(v_texcoord + offsetUV * (1.0 + spread), halfTexel, maximumUV)).b;
    } else
        colour = liquidSample(clamp(v_texcoord + offsetUV, halfTexel, maximumUV));

    float scale = max(liquidLuminanceScale, 0.001);
    vec3 glass = pow(max(colour / scale, vec3(0.0)), vec3(1.0 / 2.2));
    vec3 plain = glass;

    // Frosted: a little less colour, bright backdrops dimmed and dark ones lifted, then the tint.
    float luma = dot(glass, LIQUID_LUMA);
    glass = mix(vec3(luma), glass, clamp(liquidSaturation, 0.0, 2.0));
    float bright = smoothstep(0.25, 0.55, luma);
    glass *= 1.0 - clamp(liquidAdaptiveDim, 0.0, 1.0) * bright;
    glass += vec3(clamp(liquidAdaptiveBoost, 0.0, 1.0) * (1.0 - bright) * 0.5);
    glass = mix(glass, liquidTint.rgb, clamp(liquidTint.a, 0.0, 1.0));

    // Light on the rim, a highlight along the top edge and a shadow along the bottom one.
    float rim = edge * edge;
    float top = clamp(position.y / halfSize.y * 0.5 + 0.5, 0.0, 1.0);
    glass += vec3(rim * liquidFresnel * 0.15);
    glass += vec3(1.0, 0.99, 0.97) * (top * top) * rim * liquidSpecular * 0.08;
    glass *= 1.0 - (1.0 - top) * (1.0 - top) * rim * 0.06;

    glass = mix(plain, glass, effect);
    vec3 linear = pow(max(glass, vec3(0.0)), vec3(2.2)) * scale;
    vec4 liquid = fromLinear(vec4(linear * blurred.a, blurred.a), liquidTransferFunction);
    return mix(blurred, liquid, shape);
}

void main() {
    vec4 color = texture(tex, v_texcoord);
    if (liquidEnabled != 0)
        color = applyLiquid(color);

    fragColor = blurFinish(color, v_texcoord, noise, brightness
#if USE_CM
                           ,
                           sourceTF, targetTF, convertMatrix, srcTFRange, dstTFRange
#endif
    );
}
