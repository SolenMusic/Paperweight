#include <paperweight/generator.hpp>
#include <paperweight/hash.hpp>
#include <paperweight/image.hpp>
#include <paperweight/material.hpp>
#include <paperweight/noise.hpp>
#include <paperweight/pmat.hpp>
#include <paperweight/version.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <utility>

namespace {

int failures = 0;

void expect(bool condition, std::string_view description)
{
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
        ++failures;
    }
}

void expectNear(double actual, double expected, double tolerance, std::string_view description)
{
    expect(std::abs(actual - expected) <= tolerance, description);
}

template<typename Exception, typename Function>
void expectThrows(Function&& function, std::string_view description)
{
    try {
        function();
        expect(false, description);
    } catch (const Exception&) {
    } catch (...) {
        expect(false, description);
    }
}

std::uint64_t checksum(std::span<const paperweight::Rgba8> pixels)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto& pixel : pixels) {
        for (const auto channel : {pixel.red, pixel.green, pixel.blue, pixel.alpha}) {
            hash ^= channel;
            hash *= 1099511628211ULL;
        }
    }
    return hash;
}

void testVersion()
{
    constexpr paperweight::Version expected{0, 0, 1};
    static_assert(paperweight::currentVersion == expected);
    expect(paperweight::versionString() == "0.0.1", "version string is 0.0.1");
}

void testImage()
{
    constexpr paperweight::Rgba8 fill{10, 20, 30, 40};
    paperweight::Image image(3, 2, fill);
    expect(image.width() == 3 && image.height() == 2, "image retains its dimensions");
    expect(image.format() == paperweight::PixelFormat::rgba8Unorm, "image reports RGBA8");
    expect(image.bytesPerRow() == 12, "image reports its byte stride");
    expect(image.pixels().size() == 6, "image owns contiguous pixels");
    expect(image.row(1).front() == fill, "image rows address the expected pixels");

    image.row(1)[2] = paperweight::Rgba8{1, 2, 3, 4};
    expect(image.pixels().back() == paperweight::Rgba8{1, 2, 3, 4}, "row writes update storage");

    auto copy = image;
    expect(copy.pixels().data() != image.pixels().data() &&
               std::equal(copy.pixels().begin(), copy.pixels().end(), image.pixels().begin()),
           "image copies own independent equivalent storage");
    auto moved = std::move(copy);
    expect(moved.width() == 3 && moved.pixels().back() == paperweight::Rgba8{1, 2, 3, 4},
           "image moves retain their dimensions and pixels");

    expectThrows<std::invalid_argument>([] { paperweight::Image invalid(0, 1); },
                                        "zero-width images are rejected");
    expectThrows<std::length_error>(
        [] {
            paperweight::Image enormous(
                std::numeric_limits<std::uint32_t>::max(),
                std::numeric_limits<std::uint32_t>::max());
        },
        "unaddressable image storage is rejected before allocation");
    expectThrows<std::out_of_range>([&image] { static_cast<void>(image.row(2)); },
                                    "out-of-range rows are rejected");
}

void testHashing()
{
    const auto first = paperweight::hashCoordinates(1234, -5, 9, 2);
    expect(first == 0xb189c66deeb2ea9dULL, "coordinate hash matches its golden vector");
    expect(first == paperweight::hashCoordinates(1234, -5, 9, 2),
           "coordinate hashing is repeatable");
    expect(first != paperweight::hashCoordinates(1235, -5, 9, 2), "seed changes the hash");
    expect(first != paperweight::hashCoordinates(1234, -4, 9, 2), "coordinate changes the hash");
    expect(first != paperweight::hashCoordinates(1234, -5, 9, 3), "stream changes the hash");
    expect(paperweight::unitDouble(first) >= 0.0 && paperweight::unitDouble(first) < 1.0,
           "unit conversion stays in [0, 1)");
}

void testPeriodicNoise()
{
    constexpr double tolerance = 1.0e-12;
    for (const double x : {-7.25, -0.5, 0.0, 1.125, 8.75}) {
        for (const double y : {-3.75, 0.25, 5.5}) {
            const double sample = paperweight::periodicValueNoise2D(x, y, 7, 5, 9981);
            expectNear(sample, paperweight::periodicValueNoise2D(x + 7.0, y, 7, 5, 9981),
                       tolerance, "value noise repeats on x");
            expectNear(sample, paperweight::periodicValueNoise2D(x, y + 5.0, 7, 5, 9981),
                       tolerance, "value noise repeats on y");
        }
    }

    expectThrows<std::invalid_argument>(
        [] { static_cast<void>(paperweight::periodicValueNoise2D(0.0, 0.0, 0, 2, 1)); },
        "zero noise periods are rejected");
}

void testMaterialAndFbm()
{
    const paperweight::Material material;
    expect(!paperweight::validateMaterial(material), "default material is valid");
    const auto& gainMetadata = paperweight::metadataFor(paperweight::MaterialParameter::gain);
    expect(gainMetadata.key == "noise.gain" && gainMetadata.defaultValue == material.gain,
           "parameter metadata describes the material defaults");
    const auto& lowColourMetadata = paperweight::metadataFor(paperweight::MaterialColour::low);
    expect(lowColourMetadata.key == "colour.low" &&
               lowColourMetadata.defaultValue == material.lowColour,
           "colour metadata describes the material defaults");

    auto invalid = material;
    invalid.octaves = 0;
    expect(paperweight::validateMaterial(invalid).has_value(), "invalid octave count is diagnosed");
    invalid = material;
    invalid.gain = std::numeric_limits<double>::quiet_NaN();
    expect(paperweight::validateMaterial(invalid).has_value(), "non-finite gain is diagnosed");

    const double sample = paperweight::periodicFbm2D(-0.375, 0.625, material);
    expectNear(sample, paperweight::periodicFbm2D(0.625, 0.625, material), 1.0e-12,
               "FBM repeats after one tile on x");
    expectNear(sample, paperweight::periodicFbm2D(-0.375, 1.625, material), 1.0e-12,
               "FBM repeats after one tile on y");
    expect(sample >= 0.0 && sample <= 1.0, "normalised FBM stays in range");

    const std::array representativeMaterials{
        paperweight::Material{0, 1, 1, 1, 0.1},
        paperweight::Material{42, 7, 4, 2, 0.37},
        paperweight::Material{std::numeric_limits<std::uint64_t>::max(), 1, 7, 4, 0.9},
    };
    for (const auto& candidate : representativeMaterials) {
        for (const auto [u, v] : std::array{
                 std::pair{-2.25, -0.125},
                 std::pair{0.0, 0.0},
                 std::pair{0.375, 0.875},
                 std::pair{4.125, 9.625},
             }) {
            const double value = paperweight::periodicFbm2D(u, v, candidate);
            expectNear(value, paperweight::periodicFbm2D(u + 1.0, v, candidate), 1.0e-12,
                       "representative FBM repeats on x");
            expectNear(value, paperweight::periodicFbm2D(u, v + 1.0, candidate), 1.0e-12,
                       "representative FBM repeats on y");
        }
    }
}

void testGenerator()
{
    const paperweight::GenerationRequest request{paperweight::Material{}, 48, 32};
    auto firstResult = paperweight::generate(request);
    auto secondResult = paperweight::generate(request);
    expect(std::holds_alternative<paperweight::Image>(firstResult), "valid request generates an image");
    expect(std::holds_alternative<paperweight::Image>(secondResult), "repeated request generates an image");
    if (!std::holds_alternative<paperweight::Image>(firstResult) ||
        !std::holds_alternative<paperweight::Image>(secondResult)) {
        return;
    }

    const auto& first = std::get<paperweight::Image>(firstResult);
    const auto& second = std::get<paperweight::Image>(secondResult);
    expect(checksum(first.pixels()) == 4981563472745378647ULL,
           "default material matches its golden checksum");
    expect(first.pixels().size() == static_cast<std::size_t>(48 * 32),
           "generator returns the requested dimensions");
    expect(checksum(first.pixels()) == checksum(second.pixels()),
           "identical generation requests are byte-identical");
    expect(first.pixels().front().alpha == 255 && first.pixels().back().alpha == 255,
           "generated texture is opaque RGBA");

    auto changedRequest = request;
    changedRequest.material.seed += 1;
    const auto changedResult = paperweight::generate(changedRequest);
    expect(std::holds_alternative<paperweight::Image>(changedResult), "changed seed still generates");
    if (const auto* changed = std::get_if<paperweight::Image>(&changedResult)) {
        expect(checksum(first.pixels()) != checksum(changed->pixels()), "seed changes generated pixels");
    }

    auto colouredRequest = request;
    colouredRequest.material.lowColour = {20, 40, 80, 64};
    colouredRequest.material.highColour = {240, 180, 100, 224};
    const auto colouredA = paperweight::generate(colouredRequest);
    const auto colouredB = paperweight::generate(colouredRequest);
    const auto* colouredImageA = std::get_if<paperweight::Image>(&colouredA);
    const auto* colouredImageB = std::get_if<paperweight::Image>(&colouredB);
    expect(colouredImageA != nullptr && colouredImageB != nullptr &&
               std::equal(
                   colouredImageA->pixels().begin(),
                   colouredImageA->pixels().end(),
                   colouredImageB->pixels().begin()),
           "custom two-colour generation is byte-deterministic");
    if (colouredImageA != nullptr) {
        const auto pixel = colouredImageA->pixels().front();
        expect(pixel.red >= 20 && pixel.red <= 240 && pixel.green >= 40 && pixel.green <= 180 &&
                   pixel.blue >= 80 && pixel.blue <= 100 && pixel.alpha >= 64 && pixel.alpha <= 224,
               "generated channels interpolate within the selected colour endpoints");
    }

    for (const auto [width, height] : std::array{
             std::pair<std::uint32_t, std::uint32_t>{1, 1},
             std::pair<std::uint32_t, std::uint32_t>{17, 29},
             std::pair<std::uint32_t, std::uint32_t>{65, 3},
         }) {
        const paperweight::GenerationRequest representative{paperweight::Material{}, width, height};
        const auto generatedA = paperweight::generate(representative);
        const auto generatedB = paperweight::generate(representative);
        const auto* imageA = std::get_if<paperweight::Image>(&generatedA);
        const auto* imageB = std::get_if<paperweight::Image>(&generatedB);
        expect(imageA != nullptr && imageB != nullptr &&
                   checksum(imageA->pixels()) == checksum(imageB->pixels()),
               "representative output dimensions remain byte-deterministic");
    }

    auto invalidRequest = request;
    invalidRequest.width = 0;
    const auto invalidResult = paperweight::generate(invalidRequest);
    expect(std::holds_alternative<paperweight::GenerationError>(invalidResult),
           "invalid dimensions return a structured error");
}

void testPmat()
{
    constexpr std::string_view canonical =
        "# Paperweight procedural material\n"
        "pmat.version = 1\n"
        "material.type = fbm\n"
        "material.seed = 18431\n"
        "colour.low = 0x000000FF\n"
        "colour.high = 0xFFFFFFFF\n"
        "noise.frequency = 4\n"
        "noise.octaves = 5\n"
        "noise.lacunarity = 2\n"
        "noise.gain = 0.5\n";

    const auto parsed = paperweight::parsePmat(canonical);
    expect(std::holds_alternative<paperweight::Material>(parsed), "canonical .pmat parses");
    if (const auto* material = std::get_if<paperweight::Material>(&parsed)) {
        expect(*material == paperweight::Material{}, "canonical .pmat has default values");
        const auto serialised = paperweight::serialisePmat(*material);
        expect(std::holds_alternative<std::string>(serialised), "valid material serialises");
        if (const auto* text = std::get_if<std::string>(&serialised)) {
            expect(*text == canonical, "serialisation uses the canonical representation");
            const auto reparsed = paperweight::parsePmat(*text);
            expect(std::holds_alternative<paperweight::Material>(reparsed) &&
                       std::get<paperweight::Material>(reparsed) == *material,
                   "parse-serialise-parse preserves the material");
        }
    }

    std::ifstream exampleFile("default.pmat", std::ios::binary);
    const std::string exampleText(
        std::istreambuf_iterator<char>{exampleFile},
        std::istreambuf_iterator<char>{});
    expect(exampleFile.good() || exampleFile.eof(), "canonical example file is readable");
    const auto example = paperweight::parsePmat(exampleText);
    expect(std::holds_alternative<paperweight::Material>(example) &&
               std::get<paperweight::Material>(example) == paperweight::Material{},
           "checked-in canonical example parses as the default material");
    if (const auto* material = std::get_if<paperweight::Material>(&example)) {
        const auto generated = paperweight::generate({*material, 48, 32});
        const auto* image = std::get_if<paperweight::Image>(&generated);
        expect(image != nullptr && checksum(image->pixels()) == 4981563472745378647ULL,
               "checked-in example retains the default golden image checksum");
    }

    std::ifstream emberFile("ember.pmat", std::ios::binary);
    const std::string emberText(
        std::istreambuf_iterator<char>{emberFile},
        std::istreambuf_iterator<char>{});
    const auto ember = paperweight::parsePmat(emberText);
    const auto* emberMaterial = std::get_if<paperweight::Material>(&ember);
    expect(emberMaterial != nullptr && emberMaterial->seed == 918273 &&
               emberMaterial->lowColour == paperweight::Rgba8{22, 12, 40, 255} &&
               emberMaterial->highColour == paperweight::Rgba8{255, 179, 71, 255},
           "checked-in coloured example parses with its expected endpoints");
    if (emberMaterial != nullptr) {
        const auto firstEmber = paperweight::generate({*emberMaterial, 37, 29});
        const auto secondEmber = paperweight::generate({*emberMaterial, 37, 29});
        const auto* firstImage = std::get_if<paperweight::Image>(&firstEmber);
        const auto* secondImage = std::get_if<paperweight::Image>(&secondEmber);
        expect(firstImage != nullptr && secondImage != nullptr &&
                   std::equal(
                       firstImage->pixels().begin(),
                       firstImage->pixels().end(),
                       secondImage->pixels().begin()),
               "checked-in coloured example generates deterministically");
    }

    const std::array roundTripMaterials{
        paperweight::Material{0, 1, 1, 1, 0.1},
        paperweight::Material{927364821, 13, 4, 2, 0.37},
        paperweight::Material{std::numeric_limits<std::uint64_t>::max(), 1, 7, 4, 0.9},
        paperweight::Material{42, 8, 3, 2, 0.625, {1, 2, 3, 4}, {250, 240, 230, 220}},
    };
    for (const auto& candidate : roundTripMaterials) {
        const auto text = paperweight::serialisePmat(candidate);
        expect(std::holds_alternative<std::string>(text),
               "boundary and custom materials serialise");
        if (const auto* serialised = std::get_if<std::string>(&text)) {
            const auto roundTrip = paperweight::parsePmat(*serialised);
            expect(std::holds_alternative<paperweight::Material>(roundTrip) &&
                       std::get<paperweight::Material>(roundTrip) == candidate,
                   "boundary and custom materials round-trip exactly");
            if (const auto* reparsed = std::get_if<paperweight::Material>(&roundTrip)) {
                const auto directImage = paperweight::generate({candidate, 23, 17});
                const auto fileImage = paperweight::generate({*reparsed, 23, 17});
                const auto* directPixels = std::get_if<paperweight::Image>(&directImage);
                const auto* filePixels = std::get_if<paperweight::Image>(&fileImage);
                expect(directPixels != nullptr && filePixels != nullptr &&
                           std::equal(
                               directPixels->pixels().begin(),
                               directPixels->pixels().end(),
                               filePixels->pixels().begin()),
                       "saved definition and standalone material produce identical pixels");
            }
        }
    }

    constexpr std::string_view flexible =
        "  # Comments and CRLF are accepted\r\n"
        "noise.gain=0.75 # trailing comment\r\n"
        "noise.lacunarity = 3\r\n"
        "noise.octaves = 4\r\n"
        "noise.frequency = 7\r\n"
        "material.seed = 99\r\n"
        "material.type = fbm\r\n"
        "pmat.version = 1\r\n";
    const auto flexibleResult = paperweight::parsePmat(flexible);
    expect(std::holds_alternative<paperweight::Material>(flexibleResult),
           "comments, whitespace, CRLF, and key order are flexible");
    if (const auto* material = std::get_if<paperweight::Material>(&flexibleResult)) {
        expect(material->seed == 99 && material->frequency == 7 && material->octaves == 4 &&
                   material->lacunarity == 3 && material->gain == 0.75,
               "flexible input produces expected values");
        expect(material->lowColour == paperweight::Rgba8{0, 0, 0, 255} &&
                   material->highColour == paperweight::Rgba8{255, 255, 255, 255},
               "preview-era files without colour fields retain black-to-white defaults");
    }

    const auto expectDiagnostic = [](std::string_view text, std::size_t line, std::string_view phrase) {
        const auto result = paperweight::parsePmat(text);
        expect(std::holds_alternative<paperweight::ParseDiagnostic>(result),
               "invalid .pmat produces a diagnostic");
        if (const auto* error = std::get_if<paperweight::ParseDiagnostic>(&result)) {
            expect(error->line == line && error->column > 0 &&
                       error->message.find(phrase) != std::string::npos,
                   "diagnostic contains source position and reason");
        }
    };

    expectDiagnostic("pmat.version = 2\n", 1, "unsupported");
    expectDiagnostic("unknown.key = 1\n", 1, "unknown key");
    expectDiagnostic("pmat.version = 1\npmat.version = 1\n", 2, "duplicate");
    expectDiagnostic("pmat.version = nope\n", 1, "integer");
    expectDiagnostic("pmat.version = 1\n", 3, "missing required key");
    expectDiagnostic("pmat.version = 1 = 2\n", 1, "exactly one");
    expectDiagnostic(
        "pmat.version = 1\nmaterial.type = fbm\nmaterial.seed = 0\n"
        "colour.low = blue\nnoise.frequency = 1\nnoise.octaves = 1\n"
        "noise.lacunarity = 1\nnoise.gain = 0.5\n",
        4,
        "0xRRGGBBAA");
    expectDiagnostic(
        "pmat.version = 1\nmaterial.type = fbm\nmaterial.seed = 0\n"
        "noise.frequency = 65\nnoise.octaves = 1\nnoise.lacunarity = 1\nnoise.gain = 0.5\n",
        4,
        "frequency");

    auto invalid = paperweight::Material{};
    invalid.gain = 2.0;
    expect(std::holds_alternative<paperweight::SerialisationError>(
               paperweight::serialisePmat(invalid)),
           "invalid materials are not serialised");
}

} // namespace

int main()
{
    testVersion();
    testImage();
    testHashing();
    testPeriodicNoise();
    testMaterialAndFbm();
    testGenerator();
    testPmat();

    if (failures != 0) {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "All Paperweight tests passed\n";
    return 0;
}
