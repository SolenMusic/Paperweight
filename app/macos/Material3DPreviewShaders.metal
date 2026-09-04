#include <metal_stdlib>
using namespace metal;

struct PreviewVertex {
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;
};

struct PreviewUniforms {
    float4x4 modelViewProjection;
    float4x4 model;
    float4x4 normalMatrix;
    float4 cameraPosition;
    float4 lightDirection;
    float4 settings;
    float4 mapSettings;
    float4 toonSettings;
    float4 opticalSettings;
    float4 environmentSettings;
    float4 specialMapSettings;
    float4 anisotropySettings;
    float4 shapeSettings;
};

struct PreviewVaryings {
    float4 position [[position]];
    float3 worldPosition;
    float3 normal;
    float3 tangent;
    float tangentSign;
    float2 uv;
};

constexpr sampler materialSampler(
    coord::normalized,
    address::repeat,
    filter::linear,
    mip_filter::none);

constant float previewPi = 3.14159265358979323846;

float3 rotateEnvironment(float3 direction, float angle)
{
    const float sineAngle = sin(angle);
    const float cosineAngle = cos(angle);
    return float3(
        direction.x * cosineAngle - direction.z * sineAngle,
        direction.y,
        direction.x * sineAngle + direction.z * cosineAngle);
}

float softPanel(float3 direction, float3 panelDirection, float focus)
{
    return pow(saturate(dot(direction, normalize(panelDirection))), focus);
}

float3 studioEnvironment(float3 rawDirection, float preset, float rotation)
{
    const float3 direction = rotateEnvironment(normalize(rawDirection), rotation);
    const float horizon = 1.0 - smoothstep(0.0, 0.14, abs(direction.y));
    const float upper = saturate(direction.y * 0.5 + 0.5);

    if (preset < 0.5) {
        float3 colour = mix(float3(0.018, 0.022, 0.030), float3(0.10, 0.12, 0.16), upper);
        colour += softPanel(direction, float3(-0.72, 0.30, 0.48), 22.0) * float3(5.8, 5.5, 5.1);
        colour += softPanel(direction, float3(0.66, 0.55, 0.30), 34.0) * float3(3.6, 4.1, 4.8);
        colour += softPanel(direction, float3(0.10, -0.24, -0.97), 52.0) * float3(1.8, 1.6, 1.4);
        return colour + horizon * float3(0.28, 0.31, 0.36);
    }
    if (preset < 1.5) {
        float3 colour = mix(float3(0.035, 0.040, 0.048), float3(0.16, 0.18, 0.21), upper);
        const float stripA = pow(saturate(1.0 - abs(direction.x + 0.42)), 34.0) *
            smoothstep(-0.5, 0.7, direction.y);
        const float stripB = pow(saturate(1.0 - abs(direction.x - 0.52)), 46.0) *
            smoothstep(-0.7, 0.5, direction.y);
        colour += stripA * float3(3.8, 3.9, 4.1);
        colour += stripB * float3(2.0, 2.2, 2.6);
        return colour + horizon * float3(0.12, 0.13, 0.15);
    }
    if (preset < 2.5) {
        float3 colour = mix(float3(0.11, 0.095, 0.080), float3(0.55, 0.61, 0.70), upper);
        colour += softPanel(direction, float3(-0.50, 0.62, 0.55), 12.0) * float3(2.2, 2.0, 1.75);
        colour += softPanel(direction, float3(0.76, 0.28, 0.58), 28.0) * float3(1.1, 1.25, 1.5);
        return colour + horizon * float3(0.22, 0.19, 0.16);
    }
    if (preset < 3.5) {
        float3 colour = mix(float3(0.018, 0.024, 0.018), float3(0.30, 0.43, 0.60), upper);
        colour += softPanel(direction, float3(-0.34, 0.82, 0.45), 110.0) * float3(7.0, 6.4, 5.3);
        colour += horizon * float3(0.38, 0.42, 0.46);
        return colour;
    }

    return mix(float3(0.12, 0.12, 0.12), float3(0.48, 0.50, 0.53), upper) +
        horizon * float3(0.08);
}

float distributionGGX(float normalDotHalf, float roughness)
{
    const float alpha = roughness * roughness;
    const float alphaSquared = alpha * alpha;
    const float denominator = normalDotHalf * normalDotHalf * (alphaSquared - 1.0) + 1.0;
    return alphaSquared / max(previewPi * denominator * denominator, 0.000001);
}

float distributionAnisotropicGGX(
    float3 halfDirection,
    float3 surfaceNormal,
    float3 tangent,
    float3 bitangent,
    float roughness,
    float anisotropy)
{
    const float alpha = max(roughness * roughness, 0.002);
    const float aspect = sqrt(max(1.0 - anisotropy * 0.9, 0.1));
    const float alphaX = max(alpha / aspect, 0.002);
    const float alphaY = max(alpha * aspect, 0.002);
    const float halfX = dot(halfDirection, tangent);
    const float halfY = dot(halfDirection, bitangent);
    const float halfZ = dot(halfDirection, surfaceNormal);
    const float denominator = halfX * halfX / (alphaX * alphaX) +
        halfY * halfY / (alphaY * alphaY) + halfZ * halfZ;
    return 1.0 / max(previewPi * alphaX * alphaY * denominator * denominator, 0.000001);
}

float geometrySchlickGGX(float normalDotDirection, float roughness)
{
    const float value = roughness + 1.0;
    const float k = (value * value) * 0.125;
    return normalDotDirection / max(normalDotDirection * (1.0 - k) + k, 0.000001);
}

float3 fresnelSchlick(float cosineTheta, float3 reflectance)
{
    return reflectance + (1.0 - reflectance) * pow(saturate(1.0 - cosineTheta), 5.0);
}

vertex PreviewVaryings paperweightPreviewVertex(
    uint vertexId [[vertex_id]],
    const device PreviewVertex* vertices [[buffer(0)]],
    constant PreviewUniforms& uniforms [[buffer(1)]],
    texture2d<float> heightTexture [[texture(1)]])
{
    PreviewVertex source = vertices[vertexId];
    float3 localPosition = source.position;
    float3 localNormal = source.normal;
    float3 localTangent = source.tangent.xyz;
    if (uniforms.shapeSettings.x > 0.5) {
        const float u = source.uv.x;
        const float v = source.uv.y;
        const float phase = uniforms.shapeSettings.y * 2.0 * previewPi;
        const float envelope = smoothstep(0.0, 0.24, u);
        const float primaryAngle = 2.0 * previewPi * (u * 1.15) + phase;
        const float secondaryAngle = 2.0 * previewPi * (v * 0.72) - phase * 0.63;
        const float primaryWave = sin(primaryAngle) * 0.105;
        const float secondaryWave = sin(secondaryAngle) * 0.038;
        const float droop = u * u * 0.075;
        localPosition.z += envelope * (primaryWave + secondaryWave);
        localPosition.y -= droop;

        const float envelopeDerivative = u < 0.24
            ? 25.0 * (u / 0.24) * (1.0 - u / 0.24)
            : 0.0;
        const float dzdu = envelopeDerivative * (primaryWave + secondaryWave) +
            envelope * cos(primaryAngle) * 0.105 * 2.0 * previewPi * 1.15;
        const float dzdv = envelope * cos(secondaryAngle) * 0.038 *
            2.0 * previewPi * 0.72;
        const float3 derivativeU = float3(1.8, -2.0 * u * 0.075, dzdu);
        const float3 derivativeV = float3(0.0, -1.64, dzdv);
        localNormal = normalize(cross(derivativeV, derivativeU));
        localTangent = normalize(derivativeU);
    }
    const float height = heightTexture.sample(materialSampler, source.uv, level(0.0)).r;
    const float displacement = uniforms.mapSettings.y > 0.5
        ? (height - 0.5) * uniforms.settings.w
        : 0.0;
    localPosition += localNormal * displacement;
    const float4 worldPosition = uniforms.model * float4(localPosition, 1.0);

    PreviewVaryings output;
    output.position = uniforms.modelViewProjection * float4(localPosition, 1.0);
    output.worldPosition = worldPosition.xyz;
    output.normal = normalize((uniforms.normalMatrix * float4(localNormal, 0.0)).xyz);
    output.tangent = normalize((uniforms.normalMatrix * float4(localTangent, 0.0)).xyz);
    output.tangentSign = source.tangent.w;
    output.uv = source.uv;
    return output;
}

fragment float4 paperweightPreviewFragment(
    PreviewVaryings input [[stage_in]],
    constant PreviewUniforms& uniforms [[buffer(1)]],
    texture2d<float> colourTexture [[texture(0)]],
    texture2d<float> normalTexture [[texture(2)]],
    texture2d<float> roughnessTexture [[texture(3)]],
    texture2d<float> metalnessTexture [[texture(4)]],
    texture2d<float> coatingTexture [[texture(5)]],
    texture2d<float> occlusionTexture [[texture(6)]],
    texture2d<float> clearCoatTexture [[texture(7)]],
    texture2d<float> clearCoatRoughnessTexture [[texture(8)]],
    texture2d<float> emissiveTexture [[texture(9)]])
{
    const float3 baseColour = uniforms.mapSettings.x > 0.5
        ? colourTexture.sample(materialSampler, input.uv).rgb
        : float3(0.52);
    float roughness = uniforms.mapSettings.w > 0.5
        ? roughnessTexture.sample(materialSampler, input.uv).r
        : 0.5;
    roughness = clamp(roughness, 0.045, 1.0);
    const float metalness = uniforms.opticalSettings.x > 0.5
        ? saturate(metalnessTexture.sample(materialSampler, input.uv).r)
        : 0.0;
    const float coating = saturate(coatingTexture.sample(materialSampler, input.uv).r);
    const float occlusion = uniforms.specialMapSettings.x > 0.5
        ? saturate(occlusionTexture.sample(materialSampler, input.uv).r)
        : 1.0;
    const float clearCoat = uniforms.specialMapSettings.y > 0.5
        ? saturate(clearCoatTexture.sample(materialSampler, input.uv).r)
        : 0.0;
    const float clearCoatRoughness = clamp(
        clearCoatRoughnessTexture.sample(materialSampler, input.uv).r, 0.045, 1.0);
    const float3 emissive = uniforms.specialMapSettings.z > 0.5
        ? emissiveTexture.sample(materialSampler, input.uv).rgb
        : float3(0.0);

    float3 surfaceNormal = normalize(input.normal);
    float3 tangent = normalize(input.tangent - surfaceNormal * dot(surfaceNormal, input.tangent));
    float3 bitangent = normalize(cross(surfaceNormal, tangent)) * input.tangentSign;
    if (uniforms.mapSettings.z > 0.5) {
        float3 mapped = normalTexture.sample(materialSampler, input.uv).xyz * 2.0 - 1.0;
        mapped.xy *= uniforms.settings.z;
        mapped = normalize(mapped);
        surfaceNormal = normalize(
            tangent * mapped.x + bitangent * mapped.y + surfaceNormal * mapped.z);
        tangent = normalize(tangent - surfaceNormal * dot(surfaceNormal, tangent));
        bitangent = normalize(cross(surfaceNormal, tangent)) * input.tangentSign;
    }

    const float brushRotation = uniforms.anisotropySettings.y;
    const float brushCosine = cos(brushRotation);
    const float brushSine = sin(brushRotation);
    const float3 brushTangent = normalize(tangent * brushCosine + bitangent * brushSine);
    const float3 brushBitangent = normalize(-tangent * brushSine + bitangent * brushCosine);
    const float anisotropy = uniforms.specialMapSettings.w > 0.5
        ? saturate(uniforms.anisotropySettings.x) * (1.0 - coating)
        : 0.0;

    const float3 lightDirection = normalize(uniforms.lightDirection.xyz);
    const float3 viewDirection = normalize(uniforms.cameraPosition.xyz - input.worldPosition);
    const float3 halfDirection = normalize(lightDirection + viewDirection);
    float normalDotLight = saturate(dot(surfaceNormal, lightDirection));
    const float normalDotView = max(saturate(dot(surfaceNormal, viewDirection)), 0.0001);
    const float normalDotHalf = saturate(dot(surfaceNormal, halfDirection));
    const float viewDotHalf = saturate(dot(viewDirection, halfDirection));

    const float ior = clamp(uniforms.opticalSettings.y, 1.0, 4.0);
    const float dielectricF0Scalar = pow((ior - 1.0) / (ior + 1.0), 2.0);
    const float3 reflectance = mix(float3(dielectricF0Scalar), baseColour, metalness);
    float3 fresnel = fresnelSchlick(viewDotHalf, reflectance);
    const float distribution = mix(
        distributionGGX(normalDotHalf, roughness),
        distributionAnisotropicGGX(
            halfDirection,
            surfaceNormal,
            brushTangent,
            brushBitangent,
            roughness,
            anisotropy),
        anisotropy);
    const float geometry = geometrySchlickGGX(normalDotView, roughness) *
        geometrySchlickGGX(normalDotLight, roughness);
    float3 directSpecular = distribution * geometry * fresnel /
        max(4.0 * normalDotView * normalDotLight, 0.0001);
    float3 directDiffuse = (1.0 - fresnel) * (1.0 - metalness) * baseColour / previewPi;
    const float3 coatFresnel = fresnelSchlick(viewDotHalf, float3(0.04));
    const float coatDistribution = distributionGGX(normalDotHalf, clearCoatRoughness);
    const float coatGeometry = geometrySchlickGGX(normalDotView, clearCoatRoughness) *
        geometrySchlickGGX(normalDotLight, clearCoatRoughness);
    float3 directCoat = clearCoat * coatDistribution * coatGeometry * coatFresnel /
        max(4.0 * normalDotView * normalDotLight, 0.0001);
    const float baseAttenuation = 1.0 - clearCoat * coatFresnel.r;
    directSpecular *= baseAttenuation;
    directDiffuse *= baseAttenuation;

    if (uniforms.toonSettings.x > 0.5) {
        const float bands = max(round(uniforms.toonSettings.y), 2.0);
        normalDotLight = clamp(floor(normalDotLight * bands) / (bands - 1.0), 0.0, 1.0);
        const float highlight = step(uniforms.toonSettings.z,
            max(max(directSpecular.r, directSpecular.g), directSpecular.b));
        directSpecular = highlight * fresnel;
        directCoat = step(uniforms.toonSettings.z,
            max(max(directCoat.r, directCoat.g), directCoat.b)) * coatFresnel * clearCoat;
    }

    const float3 directRadiance = float3(uniforms.settings.x * 3.2);
    float3 colour = (directDiffuse + directSpecular + directCoat) *
        directRadiance * normalDotLight;

    const float preset = uniforms.environmentSettings.x;
    const float rotation = uniforms.opticalSettings.w;
    const float environmentIntensity = uniforms.opticalSettings.z;
    const float3 reflectionDirection = reflect(-viewDirection, surfaceNormal);
    const float3 sharpReflection = studioEnvironment(reflectionDirection, preset, rotation);
    const float brushAlignment = abs(dot(reflectionDirection, brushTangent));
    const float environmentRoughness = clamp(
        roughness * mix(1.0, 0.38 + brushAlignment * 0.42, anisotropy), 0.045, 1.0);
    const float3 broadReflection = studioEnvironment(
        normalize(mix(reflectionDirection, surfaceNormal,
            environmentRoughness * environmentRoughness)), preset, rotation);
    const float3 reflectedEnvironment = mix(
        sharpReflection, broadReflection, environmentRoughness * 0.82);
    const float3 environmentFresnel = fresnelSchlick(normalDotView, reflectance);
    const float3 environmentSpecular = reflectedEnvironment * environmentFresnel *
        mix(1.0, 0.28, roughness * roughness);
    const float3 diffuseEnvironment = studioEnvironment(surfaceNormal, preset, rotation) *
        baseColour * (1.0 - metalness) * 0.18;
    const float3 coatReflection = mix(
        sharpReflection,
        studioEnvironment(normalize(mix(reflectionDirection, surfaceNormal,
            clearCoatRoughness * clearCoatRoughness)), preset, rotation),
        clearCoatRoughness * 0.82);
    const float3 environmentCoat = coatReflection *
        fresnelSchlick(normalDotView, float3(0.04)) * clearCoat;
    colour += (environmentSpecular * baseAttenuation + diffuseEnvironment + environmentCoat) *
        environmentIntensity * occlusion;
    colour += baseColour * (1.0 - metalness) * uniforms.settings.y * occlusion;
    colour += emissive;

    if (uniforms.toonSettings.x > 0.5) {
        const float rim = smoothstep(0.55, 0.82, 1.0 - normalDotView) * uniforms.toonSettings.w;
        colour += fresnelSchlick(normalDotView, reflectance) * rim;
    }

    return float4(max(colour, float3(0.0)), 1.0);
}
