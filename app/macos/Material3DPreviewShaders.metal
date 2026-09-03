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
    const float height = heightTexture.sample(materialSampler, source.uv, level(0.0)).r;
    const float displacement = uniforms.mapSettings.y > 0.5
        ? (height - 0.5) * uniforms.settings.w
        : 0.0;
    const float3 localPosition = source.position + source.normal * displacement;
    const float4 worldPosition = uniforms.model * float4(localPosition, 1.0);

    PreviewVaryings output;
    output.position = uniforms.modelViewProjection * float4(localPosition, 1.0);
    output.worldPosition = worldPosition.xyz;
    output.normal = normalize((uniforms.normalMatrix * float4(source.normal, 0.0)).xyz);
    output.tangent = normalize((uniforms.normalMatrix * float4(source.tangent.xyz, 0.0)).xyz);
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
    texture2d<float> metalnessTexture [[texture(4)]])
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

    float3 surfaceNormal = normalize(input.normal);
    if (uniforms.mapSettings.z > 0.5) {
        const float3 tangent = normalize(input.tangent - surfaceNormal * dot(surfaceNormal, input.tangent));
        const float3 bitangent = normalize(cross(surfaceNormal, tangent)) * input.tangentSign;
        float3 mapped = normalTexture.sample(materialSampler, input.uv).xyz * 2.0 - 1.0;
        mapped.xy *= uniforms.settings.z;
        mapped = normalize(mapped);
        surfaceNormal = normalize(
            tangent * mapped.x + bitangent * mapped.y + surfaceNormal * mapped.z);
    }

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
    const float distribution = distributionGGX(normalDotHalf, roughness);
    const float geometry = geometrySchlickGGX(normalDotView, roughness) *
        geometrySchlickGGX(normalDotLight, roughness);
    float3 directSpecular = distribution * geometry * fresnel /
        max(4.0 * normalDotView * normalDotLight, 0.0001);
    float3 directDiffuse = (1.0 - fresnel) * (1.0 - metalness) * baseColour / previewPi;

    if (uniforms.toonSettings.x > 0.5) {
        const float bands = max(round(uniforms.toonSettings.y), 2.0);
        normalDotLight = clamp(floor(normalDotLight * bands) / (bands - 1.0), 0.0, 1.0);
        const float highlight = step(uniforms.toonSettings.z,
            max(max(directSpecular.r, directSpecular.g), directSpecular.b));
        directSpecular = highlight * fresnel;
    }

    const float3 directRadiance = float3(uniforms.settings.x * 3.2);
    float3 colour = (directDiffuse + directSpecular) * directRadiance * normalDotLight;

    const float preset = uniforms.environmentSettings.x;
    const float rotation = uniforms.opticalSettings.w;
    const float environmentIntensity = uniforms.opticalSettings.z;
    const float3 reflectionDirection = reflect(-viewDirection, surfaceNormal);
    const float3 sharpReflection = studioEnvironment(reflectionDirection, preset, rotation);
    const float3 broadReflection = studioEnvironment(
        normalize(mix(reflectionDirection, surfaceNormal, roughness * roughness)), preset, rotation);
    const float3 reflectedEnvironment = mix(sharpReflection, broadReflection, roughness * 0.82);
    const float3 environmentFresnel = fresnelSchlick(normalDotView, reflectance);
    const float3 environmentSpecular = reflectedEnvironment * environmentFresnel *
        mix(1.0, 0.28, roughness * roughness);
    const float3 diffuseEnvironment = studioEnvironment(surfaceNormal, preset, rotation) *
        baseColour * (1.0 - metalness) * 0.18;
    colour += (environmentSpecular + diffuseEnvironment) * environmentIntensity;
    colour += baseColour * (1.0 - metalness) * uniforms.settings.y;

    if (uniforms.toonSettings.x > 0.5) {
        const float rim = smoothstep(0.55, 0.82, 1.0 - normalDotView) * uniforms.toonSettings.w;
        colour += fresnelSchlick(normalDotView, reflectance) * rim;
    }

    return float4(max(colour, float3(0.0)), 1.0);
}
