/*
* Copyright 2021 Jason Scott Cohen.  All rights reserved.
*
* Licensed under the Apache License v2.0 with LLVM Exceptions.
* See https://llvm.org/LICENSE.txt for license information.
* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
*/

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

#include "simple_svg_1.0.0.hpp"

#include "CommandLine.h"

#include <memory>
#include <filesystem>
#include <chrono>

using namespace std::literals;
using namespace std::chrono;

auto& now = steady_clock::now;

struct Options : CommandLine::Parser
{
    Value<string> inputFile    {is, "-i", "--inputFile",         "Name of input file, a raster image (the \"-i\" is optional)."};
    Value<string> outputFile   {is, "-o", "--outputFile",        "Name of output file, an SVG file (default is input file changed to .svg)."};
    Value<double> scale        {is, "-s", "--scale",       10.0, "Scale factor. Default is 1."};
    Value<double> strokeWidth  {is, "-w", "--strokeWidth", 0.01, "Width of strokes to use for all paths."};
    Option        help         {is, "-h", "--help",              "Show this help text."};

    bool Validate() override
    {
        // Allow inputFile to be passed as the first positional arg
        if (!inputFile.specified && otherArgs.size() == 1)
        {
            inputFile.value = otherArgs[0];
            inputFile.specified = true;
        }
        else if (!inputFile.specified)  return false;  // Input file is required
        else if (otherArgs.size() != 0) return false;  // No positional args expected aside from implicit -i

        if (!outputFile.specified)
        {
            // Derive output name from input name
            std::filesystem::path path(inputFile.value);
            path.replace_extension("svg");
            outputFile.value = path.string();
        }

        if (scale <= 0.0) return false;
        if (strokeWidth < 0.0) return false;  // 0 is allowed

        return true;
    }
} g_opts;

class RasterImage
{
public:
    using PixData_t = unsigned char;

private:
    int width{};
    int height{};
    int channels{};
    std::unique_ptr<PixData_t[], decltype(&stbi_image_free)> img{ nullptr, stbi_image_free };
    std::string failureMessage;

public:
    RasterImage() = default;

    RasterImage(RasterImage&& other) noexcept = default;
    RasterImage& operator=(RasterImage&& other) noexcept = default;

    RasterImage(char const* inputFile)
        : img(stbi_load(inputFile, &width, &height, &channels, 0), stbi_image_free)
        , failureMessage(img ? std::string{} : stbi_failure_reason())
    {
    }

    RasterImage(std::string const& inputFile) : RasterImage(inputFile.c_str()) {}

    bool Valid() const noexcept { return bool(img); }

    // Message from an attempt to load that failed.
    std::string const& FailureReason() const noexcept { return failureMessage; }

    int Width() const noexcept { return width; }
    int Height() const noexcept { return height; }
    int ChannelCount() const noexcept { return channels; }

    bool HasColor() const noexcept { return channels >= 3; }
    bool HasAlpha() const noexcept { return channels == 2 || channels == 4; }

    PixData_t* PixelNative(int row, int col) noexcept
    {
        int index = row * width + col;
        return &img[index * channels];
    };

    PixData_t const* PixelNative(int row, int col) const noexcept
    {
        int index = row * width + col;
        return &img[index * channels];
    };

    struct RGBA
    {
        uint8_t red;
        uint8_t green;
        uint8_t blue;
        uint8_t alpha;
    };

    RGBA PixelRGBA(int row, int col) const noexcept
    {
        auto* p = PixelNative(row, col);
        switch (channels)
        {
        case 1: // gray
            return RGBA{ p[0], p[0], p[0], 0xFF };
        case 2: // gray, alpha
            return RGBA{ p[0], p[0], p[0], p[1] };
        case 3: // red, green, blue
            return RGBA{ p[0], p[1], p[2], 0xFF };
        case 4: // red, green, blue, alpha
            return RGBA{ p[0], p[1], p[2], p[3] };
        default:
            // Undefined behavior
            return RGBA{};
        }
    };
};

svg::Document RasterPixelsToSvg(RasterImage const& img, std::string const& outputFile)
{
    using namespace svg;

    Dimensions dimensions(
        g_opts.scale * img.Width(),
        g_opts.scale * img.Height());
    Document doc(outputFile, Layout(dimensions, Layout::TopLeft, g_opts.scale));

    auto startTime = now();
    std::chrono::nanoseconds estTotalDur, totalDur;
    bool slow = false;

    for (int r = 0; r < img.Height(); ++r)
    {
        for (int c = 0; c < img.Width(); ++c)
        {
            auto pixelColor = img.PixelRGBA(r, c);

            // simple-svg library doesn't support alpha, just fully-transparent.
            // Force any pixels that aren't fully opaque to be transparent.
            Color color = pixelColor.alpha < 0xFF
                ? Color::Transparent
                : Color(pixelColor.red, pixelColor.green, pixelColor.blue);

            Polygon pixel(Fill(color), Stroke(g_opts.strokeWidth, Color::Black));
            pixel
                << Point(c,     r    )
                << Point(c + 1, r    )
                << Point(c + 1, r + 1)
                << Point(c,     r + 1);
            doc << pixel;
        }

        if (r == 0)
        {
            auto oneRowDur = now() - startTime;
            estTotalDur = img.Height() * oneRowDur;

            if (estTotalDur > 2s)
            {
                slow = true;
                std::cout << "Estimated path construction time: "
                    << duration_cast<seconds>(estTotalDur).count() << " seconds\n";
            }
        }
    }

    totalDur = now() - startTime;

    if (slow)
    {
        auto percentOffFromEstimate =
            100 * double(totalDur.count() - estTotalDur.count())
            / totalDur.count();
        std::cout << "Actual path construction time:    "
            << duration_cast<seconds>(totalDur).count() << " seconds ("
            << percentOffFromEstimate << "% difference from estimate)\n";
    }
    else
    {
        auto durMs = duration_cast<milliseconds>(totalDur).count();

        std::cout << "Path construction time: " << durMs << " ms\n";
    }

    return doc;
}

int main(int argc, const char** argv)
{
    if (!g_opts.Parse(argv) || g_opts.help)
    {
        g_opts.ShowHelp(std::cout);
        return g_opts.help ? 0 : 1;
    }

    std::cout << "Converting " << g_opts.inputFile << " to " << g_opts.outputFile << ".\n"
        << "Loading input image...\n";

    RasterImage img(g_opts.inputFile);

    std::cout << "Image is " << img.Width() << "x" << img.Height()
        << ", with " << img.ChannelCount() << " color channels.\n";

    auto svgDoc = RasterPixelsToSvg(img, g_opts.outputFile);

    std::cout << "SVG paths generated.  Writing output .svg file...\n";

    bool success = svgDoc.save();
    if (!success)
    {
        std::cout << "File output failed!\n";
        return 1;
    }

    std::cout << "Completed successfully.\n";
    return 0;
}