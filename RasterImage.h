#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdint.h>
#include <memory>
#include <string>

class RasterImage
{
public:
    using PixData_t = unsigned char;

private:
    using ptr_t = std::unique_ptr<PixData_t[], decltype(&stbi_image_free)>;

    int width{};
    int height{};
    int channels{};
    ptr_t img{nullptr, stbi_image_free};
    std::string failureMessage;

public:
    RasterImage() = default;

    // Copy functionality disabled to avoid inadvertent copies.
    // Use the Clone() method to copy an instance explicitly.
    RasterImage(RasterImage const& other) noexcept = delete;
    RasterImage& operator=(RasterImage const& other) noexcept = delete;

    RasterImage(RasterImage&& other) noexcept = default;
    RasterImage& operator=(RasterImage&& other) noexcept = default;

    RasterImage(char const* inputFile)
        : img(stbi_load(inputFile, &width, &height, &channels, 0), stbi_image_free)
        , failureMessage(img ? "" : stbi_failure_reason())
    {
    }

    RasterImage(std::string const& inputFile) : RasterImage(inputFile.c_str()) {}

    RasterImage(int width_, int height_, int channels_)
        : width(width_)
        , height(height_)
        , channels(channels_)
        , img((PixData_t*)stbi__malloc_mad3(width_, height_, channels_, 0), stbi_image_free)
        , failureMessage(img ? "" : "Out of memory")
    {
        memset(img.get(), 0, width_ * height_ * channels_);
    }

    bool Valid() const noexcept { return bool(img); }

    // Error message from last attempt to load or create an image.
    // Empty if there was no error.
    std::string const& FailureReason() const noexcept { return failureMessage; }

    // Provide non-throwing swap
    
    void swap(RasterImage& rhs) noexcept
    {
        using std::swap;
        swap(width,          rhs.width);
        swap(height,         rhs.height);
        swap(channels,       rhs.channels);
        swap(img,            rhs.img);
        swap(failureMessage, rhs.failureMessage);
    }

    friend void swap(RasterImage& lhs, RasterImage& rhs) noexcept
    {
        lhs.swap(rhs);
    }

    // Replace current image with a default-constructed (empty) image, freeing memory

    void Release() noexcept
    {
        RasterImage temp;
        swap(temp);
    }

    // Replace current image with data loaded from an input file in any supported format

    bool Load(char const* inputFile) noexcept
    {
        RasterImage temp{inputFile};
        swap(temp);
        return Valid();
    }

    bool Load(std::string const& inputFile) noexcept { return Load(inputFile.c_str()); }

    // Write current image data to a BMP file

    bool SaveBmp(char const* outputFile) const noexcept
    {
        int result = stbi_write_bmp(outputFile, width, height, channels, img.get());
        return result != 0;
    }

    bool SaveBmp(std::string const& outputFile) const noexcept { return SaveBmp(outputFile.c_str()); }

    // Replace current image with a new blank image, using given dimensions and format

    bool Create(int width_, int height_, int channels_) noexcept
    {
        RasterImage temp{width_, height_, channels_};
        swap(temp);
        return Valid();
    }

    // Explicit function to copy a RasterImage

    RasterImage Clone() const noexcept
    {
        RasterImage temp{width, height, channels};
        memcpy(temp.img.get(), img.get(), SizeInBytes());
        return temp;
    }

    // Conversions that create a new RasterImage in a different format

    RasterImage AsRGB() const noexcept
    {
        RasterImage temp{width, height, 3};

        switch (channels)
        {
        case 1: // gray
        case 2: // gray, alpha
            // Assign gray value to R, G, and B -- ignore alpha
            for (int row = 0; row < height; ++row)
            {
                for (int col = 0; col < width; ++col)
                {
                    auto* in = Pixel(row, col);
                    auto* out = temp.Pixel(row, col);
                    out[0] = in[0];
                    out[1] = in[0];
                    out[2] = in[0];
                }
            }
            break;
        case 3: // red, green, blue
        case 4: // red, green, blue, alpha
            // Copy R, G, and B values -- ignore alpha
            for (int row = 0; row < height; ++row)
            {
                for (int col = 0; col < width; ++col)
                {
                    auto* in = Pixel(row, col);
                    auto* out = temp.Pixel(row, col);
                    out[0] = in[0];
                    out[1] = in[1];
                    out[2] = in[2];
                }
            }
            break;
        // default: Undefined behavior
        }

        return temp;
    }

    RasterImage AsGrayscale() const noexcept
    {
        RasterImage temp{width, height, 1};

        switch (channels)
        {
        case 1: // gray
        case 2: // gray, alpha
            // Copy brightness values -- ignore alpha
            for (int row = 0; row < height; ++row)
            {
                for (int col = 0; col < width; ++col)
                {
                    auto* in = Pixel(row, col);
                    auto* out = temp.Pixel(row, col);
                    out[0] = in[0];
                }
            }
            break;
        case 3: // red, green, blue
        case 4: // red, green, blue, alpha
            // Set brightness to average of R, G, and B -- ignore alpha
            for (int row = 0; row < height; ++row)
            {
                for (int col = 0; col < width; ++col)
                {
                    auto* in = Pixel(row, col);
                    auto* out = temp.Pixel(row, col);
                    out[0] = ToGrayscale(RGB{in[0], in[1], in[2]});
                }
            }
            break;
        // default: Undefined behavior
        }

        return temp;
    }

    // In-place conversions that change the current image's format

    RasterImage& ConvertToRGB() noexcept
    {
        RasterImage temp{AsRGB()};
        swap(temp);
        return *this;
    }

    RasterImage& ConvertToGrayscale() noexcept
    {
        RasterImage temp{AsGrayscale()};
        swap(temp);
        return *this;
    }

    // Accessors and helpers for width/height/channels

    int Width() const noexcept { return width; }
    int Height() const noexcept { return height; }
    int ChannelCount() const noexcept { return channels; }
    
    int SizeInBytes() const noexcept { return width * height * channels; }

    bool HasColor() const noexcept { return channels >= 3; }
    bool HasAlpha() const noexcept { return channels == 2 || channels == 4; }

    // Helpers to ensure row/column index is in-bounds

    int ClampRow(int row) const noexcept
    {
        return
            (row < 0) ? 0 :
            (row >= height) ? (height - 1) :
            row;
    }

    int ClampCol(int col) const noexcept
    {
        return
            (col < 0) ? 0 :
            (col >= width) ? (width - 1) :
            col;
    }

    // Direct access to pixel data in memory

    PixData_t* Pixel(int row, int col) noexcept
    {
        int index = row * width + col;
        return &img[index * channels];
    };

    PixData_t const* Pixel(int row, int col) const noexcept
    {
        int index = row * width + col;
        return &img[index * channels];
    };

    PixData_t* PixelClamped(int row, int col) noexcept
    {
        return Pixel(ClampRow(row), ClampCol(col));
    };

    PixData_t const* PixelClamped(int row, int col) const noexcept
    {
        return Pixel(ClampRow(row), ClampCol(col));
    };

    // Types to represent color pixel data

    struct RGB
    {
        PixData_t r;
        PixData_t g;
        PixData_t b;
    };

    struct RGBA
    {
        PixData_t r;
        PixData_t g;
        PixData_t b;
        PixData_t a;
    };

    // Pixel data conversion helpers

    static PixData_t ToGrayscale(RGB v) noexcept
    {
        using T = uint16_t;
        T grayscale = (T(v.r) + T(v.g) + T(v.b)) / T(3);
        return PixData_t(grayscale);
    }

    static PixData_t ToGrayscale(RGBA v) noexcept
    {
        using T = uint16_t;
        T grayscale = (T(v.r) + T(v.g) + T(v.b)) / T(3);
        return PixData_t(grayscale);
    }

    static RGB ToRGB(RGBA v) noexcept
    {
        return {v.r, v.g, v.b};
    }

    static RGBA ToRGBA(RGB v) noexcept
    {
        return {v.r, v.g, v.b, 0xFF};
    }

    // Read pixel data into a struct, converting format if necessary

    RGB GetPixelRGB(int row, int col) const noexcept
    {
        auto* p = Pixel(row, col);
        switch (channels)
        {
        case 1: // gray
        case 2: // gray, alpha
            return RGB{ p[0], p[0], p[0] };
        case 3: // red, green, blue
        case 4: // red, green, blue, alpha
            return RGB{ p[0], p[1], p[2] };
        default:
            // Undefined behavior
            return {};
        }
    };

    RGB GetPixelRGBClamped(int row, int col) const noexcept
    {
        return GetPixelRGB(ClampRow(row), ClampCol(col));
    }

    RGBA GetPixelRGBA(int row, int col) const noexcept
    {
        auto* p = Pixel(row, col);
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
            return {};
        }
    };

    RGBA GetPixelRGBAClamped(int row, int col) const noexcept
    {
        return GetPixelRGBA(ClampRow(row), ClampCol(col));
    }

    // Write pixel data from a struct, converting format if necessary

    void SetPixelRGB(int row, int col, RGB val) noexcept
    {
        auto* p = Pixel(row, col);
        switch (channels)
        {
        case 1: // gray
            p[0] = ToGrayscale(val);
            return;
        case 2: // gray, alpha
            p[0] = ToGrayscale(val);
            p[1] = 0xFF;
            return;
        case 3: // red, green, blue
            p[0] = val.r;
            p[1] = val.g;
            p[2] = val.b;
            return;
        case 4: // red, green, blue, alpha
            p[0] = val.r;
            p[1] = val.g;
            p[2] = val.b;
            p[3] = 0xFF;
            return;
        // default: Undefined behavior
        }
    };

    void SetPixelRGBClamped(int row, int col, RGB val) noexcept
    {
        SetPixelRGB(ClampRow(row), ClampCol(col), val);
    }

    void SetPixelRGBA(int row, int col, RGBA val) noexcept
    {
        auto* p = Pixel(row, col);
        switch (channels)
        {
        case 1: // gray
            p[0] = ToGrayscale(val);
            return;
        case 2: // gray, alpha
            p[0] = ToGrayscale(val);
            p[1] = val.a;
            return;
        case 3: // red, green, blue
            p[0] = val.r;
            p[1] = val.g;
            p[2] = val.b;
            return;
        case 4: // red, green, blue, alpha
            p[0] = val.r;
            p[1] = val.g;
            p[2] = val.b;
            p[3] = val.a;
            return;
        // default: Undefined behavior
        }
    };

    void SetPixelRGBAClamped(int row, int col, RGBA val) noexcept
    {
        SetPixelRGBA(ClampRow(row), ClampCol(col), val);
    }
};
