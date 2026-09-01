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

vertex PreviewVaryings paperweightPreviewVertex(
    uint vertexId [[vertex_id]],
    const device PreviewVertex* vertices [[buffer(0)]],
    constant PreviewUniforms& uniforms [[buffer(1)]],
    texture2d<float> heightTexture [[texture(1)]])
{
    PreviewVertex source = vertices[vertexId];
    float height = heightTexture.sample(materialSampler, source.uv, level(0.0)).r;
    float displacement = uniforms.mapSettings.y > 0.5
        ? (height - 0.5) * uniforms.settings.w
        : 0.0;
    float3 localPosition = source.position + source.normal * displacement;
    float4 worldPosition = uniforms.model * float4(localPosition, 1.0);

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
    texture2d<float> roughnessTexture [[texture(3)]])
{
    float3 baseColour = uniforms.mapSettings.x > 0.5
        ? colourTexture.sample(materialSampler, input.uv).rgb
        : float3(0.52);
    float roughness = uniforms.mapSettings.w > 0.5
        ? roughnessTexture.sample(materialSampler, input.uv).r
        : 0.5;
    roughness = clamp(roughness, 0.04, 1.0);

    float3 surfaceNormal = normalize(input.normal);
    if (uniforms.mapSettings.z > 0.5) {
        float3 tangent = normalize(input.tangent - surfaceNormal * dot(surfaceNormal, input.tangent));
        float3 bitangent = normalize(cross(surfaceNormal, tangent)) * input.tangentSign;
        float3 mapped = normalTexture.sample(materialSampler, input.uv).xyz * 2.0 - 1.0;
        mapped.xy *= uniforms.settings.z;
        mapped = normalize(mapped);
        surfaceNormal = normalize(
            tangent * mapped.x + bitangent * mapped.y + surfaceNormal * mapped.z);
    }

    float3 lightDirection = normalize(uniforms.lightDirection.xyz);
    float3 viewDirection = normalize(uniforms.cameraPosition.xyz - input.worldPosition);
    float diffuse = max(dot(surfaceNormal, lightDirection), 0.0);
    float3 halfDirection = normalize(lightDirection + viewDirection);
    float shininess = mix(180.0, 4.0, roughness * roughness);
    float specular = pow(max(dot(surfaceNormal, halfDirection), 0.0), shininess);
    float specularStrength = mix(0.62, 0.035, roughness);
    float ambient = uniforms.settings.y;
    float intensity = uniforms.settings.x;
    float rim = pow(1.0 - max(dot(surfaceNormal, viewDirection), 0.0), 3.0) * 0.08;

    if (uniforms.toonSettings.x > 0.5) {
        float bands = max(round(uniforms.toonSettings.y), 2.0);
        diffuse = floor(diffuse * bands) / (bands - 1.0);
        diffuse = clamp(diffuse, 0.0, 1.0);
        specular = step(uniforms.toonSettings.z, specular);
        rim = smoothstep(0.55, 0.82,
            1.0 - max(dot(surfaceNormal, viewDirection), 0.0)) * uniforms.toonSettings.w;
    }

    float3 colour = baseColour * (ambient + diffuse * intensity);
    colour += float3(specular * specularStrength * intensity + rim);
    return float4(max(colour, float3(0.0)), 1.0);
}
