/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//! \file

#ifndef TNT_FILAMENT_TEXTURE_H
#define TNT_FILAMENT_TEXTURE_H

#include <filament/FilamentAPI.h>

#include <backend/DriverEnums.h>
#include <backend/PixelBufferDescriptor.h>
#include <backend/Platform.h>

#include <utils/compiler.h>
#include <utils/StaticString.h>

#include <utility>

#include <stddef.h>
#include <stdint.h>

namespace filament {

class FTexture;

class Engine;
class Stream;

/**
 * Texture
 *
 * The Texture class supports:
 *  - 2D textures
 *  - 3D textures
 *  - Cube maps
 *  - mip mapping
 *
 *
 * Creation and destruction
 * ========================
 *
 * A Texture object is created using the Texture::Builder and destroyed by calling
 * Engine::destroy(const Texture*).
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
 *  filament::Engine* engine = filament::Engine::create();
 *
 *  filament::Texture* texture = filament::Texture::Builder()
 *              .width(64)
 *              .height(64)
 *              .build(*engine);
 *
 *  engine->destroy(texture);
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
class UTILS_PUBLIC Texture : public FilamentAPI {
    struct BuilderDetails;

public:
    static constexpr size_t BASE_LEVEL = 0;

    //! Face offsets for all faces of a cubemap
    struct FaceOffsets;

    using PixelBufferDescriptor = backend::PixelBufferDescriptor;    //!< Geometry of a pixel buffer
    using Sampler = backend::SamplerType;                            //!< Type of sampler
    using InternalFormat = backend::TextureFormat;                   //!< Internal texel format
    using CubemapFace = backend::TextureCubemapFace;                 //!< Cube map faces
    using Format = backend::PixelDataFormat;                         //!< Pixel color format
    using Type = backend::PixelDataType;                             //!< Pixel data format
    using CompressedType = backend::CompressedPixelDataType;         //!< Compressed pixel data format
    using Usage = backend::TextureUsage;                             //!< Usage affects texel layout
    using Swizzle = backend::TextureSwizzle;                         //!< Texture swizzle
    using ExternalImageHandle = backend::Platform::ExternalImageHandle;
    using ExternalImageHandleRef = backend::Platform::ExternalImageHandleRef;

    /** @return Whether a backend supports a particular format. */
    static bool isTextureFormatSupported(Engine& engine, InternalFormat format) noexcept;

    /** @return Whether a backend supports mipmapping of a particular format. */
    static bool isTextureFormatMipmappable(Engine& engine, InternalFormat format) noexcept;

    /** @return Whether particular format is compressed */
    static bool isTextureFormatCompressed(InternalFormat format) noexcept;

    /** @return Whether this backend supports protected textures. */
    static bool isProtectedTexturesSupported(Engine& engine) noexcept;

    /** @return Whether a backend supports texture swizzling. */
    static bool isTextureSwizzleSupported(Engine& engine) noexcept;

    static size_t computeTextureDataSize(Format format, Type type,
            size_t stride, size_t height, size_t alignment) noexcept;

    /** @return Whether a combination of texture format, pixel format and type is valid. */
    static bool validatePixelFormatAndType(InternalFormat internalFormat, Format format, Type type) noexcept;

    /** @return the maximum size in texels of a texture of type \p type. At least 2048 for
     * 2D textures, 256 for 3D textures. */
    static size_t getMaxTextureSize(Engine& engine, Sampler type) noexcept;

    /** @return the maximum number of layers supported by texture arrays. At least 256. */
    static size_t getMaxArrayTextureLayers(Engine& engine) noexcept;

    //! Use Builder to construct a Texture object instance
    class Builder : public BuilderBase<BuilderDetails>, public BuilderNameMixin<Builder> {
        friend struct BuilderDetails;
    public:
        Builder() noexcept;
        Builder(Builder const& rhs) noexcept;
        Builder(Builder&& rhs) noexcept;
        ~Builder() noexcept;
        Builder& operator=(Builder const& rhs) noexcept;
        Builder& operator=(Builder&& rhs) noexcept;

        /**
         * Specifies the width in texels of the texture. Doesn't need to be a power-of-two.
         * @param width Width of the texture in texels (default: 1).
         * @return This Builder, for chaining calls.
         */
        Builder& width(uint32_t width) noexcept;

        /**
         * Specifies the height in texels of the texture. Doesn't need to be a power-of-two.
         * @param height Height of the texture in texels (default: 1).
         * @return This Builder, for chaining calls.
         */
        Builder& height(uint32_t height) noexcept;

        /**
         * Specifies the depth in texels of the texture. Doesn't need to be a power-of-two.
         * The depth controls the number of layers in a 2D array texture. Values greater than 1
         * effectively create a 3D texture.
         * @param depth Depth of the texture in texels (default: 1).
         * @return This Builder, for chaining calls.
         * @attention This Texture instance must use Sampler::SAMPLER_3D or
         *            Sampler::SAMPLER_2D_ARRAY or it has no effect.
         */
        Builder& depth(uint32_t depth) noexcept;

        /**
         * Specifies the numbers of mip map levels.
         * This creates a mip-map pyramid. The maximum number of levels a texture can have is
         * such that max(width, height, level) / 2^MAX_LEVELS = 1
         * @param levels Number of mipmap levels for this texture.
         * @return This Builder, for chaining calls.
         */
        Builder& levels(uint8_t levels) noexcept;

        /**
         * Specifies the numbers of samples used for MSAA (Multisample Anti-Aliasing).
         *
         * Calling this method implicitly indicates the texture is used as a render target. Hence,
         * this method should not be used in conjunction with other methods that are semantically
         * conflicting like `setImage`.
         *
         * If this is invoked for array textures, it means this texture is used for multiview.
         *
         * @param samples Number of samples for this texture.
         * @return This Builder, for chaining calls.
         */
        Builder& samples(uint8_t samples) noexcept;

        /**
         * Specifies the type of sampler to use.
         * @param target Sampler type
         * @return This Builder, for chaining calls.
         * @see Sampler
         */
        Builder& sampler(Sampler target) noexcept;

        /**
         * Specifies the *internal* format of this texture.
         *
         * The internal format specifies how texels are stored (which may be different from how
         * they're specified in setImage()). InternalFormat specifies both the color components
         * and the data type used.
         *
         * @param format Format of the texture's texel.
         * @return This Builder, for chaining calls.
         * @see InternalFormat, setImage
         */
        Builder& format(InternalFormat format) noexcept;

        /**
         * Specifies if the texture will be used as a render target attachment.
         *
         * If the texture is potentially rendered into, it may require a different memory layout,
         * which needs to be known during construction.
         *
         * @param usage Defaults to Texture::Usage::DEFAULT; c.f. Texture::Usage::COLOR_ATTACHMENT.
         * @return This Builder, for chaining calls.
         */
        Builder& usage(Usage usage) noexcept;

        /**
         * Specifies how a texture's channels map to color components
         *
         * Texture Swizzle is only supported if isTextureSwizzleSupported() returns true.
         *
         * @param r  texture channel for red component
         * @param g  texture channel for green component
         * @param b  texture channel for blue component
         * @param a  texture channel for alpha component
         * @return This Builder, for chaining calls.
         * @see Texture::isTextureSwizzleSupported()
         */
        Builder& swizzle(Swizzle r, Swizzle g, Swizzle b, Swizzle a) noexcept;

        /**
         * Associate an optional name with this Texture for debugging purposes.
         *
         * name will show in error messages and should be kept as short as possible. The name is
         * truncated to a maximum of 128 characters.
         *
         * The name string is copied during this method so clients may free its memory after
         * the function returns.
         *
         * @param name A string to identify this Texture
         * @param len Length of name, should be less than or equal to 128
         * @return This Builder, for chaining calls.
         * @deprecated Use name(utils::StaticString const&) instead.
         */
        UTILS_DEPRECATED
        Builder& name(const char* UTILS_NONNULL name, size_t len) noexcept;

        /**
         * Associate an optional name with this Texture for debugging purposes.
         *
         * name will show in error messages and should be kept as short as possible.
         *
         * @param name A string literal to identify this Texture
         * @return This Builder, for chaining calls.
         */
        Builder& name(utils::StaticString const& name) noexcept;

        /**
         * Creates an external texture. The content must be set using setExternalImage().
         * The sampler can be SAMPLER_EXTERNAL or SAMPLER_2D depending on the format. Generally
         * YUV formats must use SAMPLER_EXTERNAL. This depends on the backend features and is not
         * validated.
         *
         * If the Sampler is set to SAMPLER_EXTERNAL, external() is implied.
         *
         * @return
         */
        Builder& external() noexcept;

        /**
         * Creates the Texture object and returns a pointer to it.
         *
         * @param engine Reference to the filament::Engine to associate this Texture with.
         *
         * @return pointer to the newly created object.
         *
         * @exception utils::PostConditionPanic if a runtime error occurred, such as running out of
         *            memory or other resources.
         * @exception utils::PreConditionPanic if a parameter to a builder function was invalid.
         */
        Texture* UTILS_NONNULL build(Engine& engine);

        /* no user serviceable parts below */

        /**
         * Specify a native texture to import as a Filament texture.
         *
         * The texture id is backend-specific:
         *   - OpenGL: GLuint texture ID
         *   - Metal: id<MTLTexture>
         *
         * With Metal, the id<MTLTexture> object should be cast to an intptr_t using
         * CFBridgingRetain to transfer ownership to Filament. Filament will release ownership of
         * the texture object when the Filament texture is destroyed.
         *
         * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
         *  id <MTLTexture> metalTexture = ...
         *  filamentTexture->import((intptr_t) CFBridgingRetain(metalTexture));
         *  // free to release metalTexture
         *
         *  // after using texture:
         *  engine->destroy(filamentTexture);   // metalTexture is released
         * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
         *
         * @warning This method should be used as a last resort. This API is subject to change or
         * removal.
         *
         * @param id a backend specific texture identifier
         *
         * @return This Builder, for chaining calls.
         */
        Builder& import(intptr_t id) noexcept;

    private:
        friend class FTexture;
    };

    /**
     * Returns the width of a 2D or 3D texture level
     * @param level texture level.
     * @return Width in texel of the specified \p level, clamped to 1.
     * @attention If this texture is using Sampler::SAMPLER_EXTERNAL, the dimension
     * of the texture are unknown and this method always returns whatever was set on the Builder.
     */
    size_t getWidth(size_t level = BASE_LEVEL) const noexcept;

    /**
     * Returns the height of a 2D or 3D texture level
     * @param level texture level.
     * @return Height in texel of the specified \p level, clamped to 1.
     * @attention If this texture is using Sampler::SAMPLER_EXTERNAL, the dimension
     * of the texture are unknown and this method always returns whatever was set on the Builder.
     */
    size_t getHeight(size_t level = BASE_LEVEL) const noexcept;

    /**
     * Returns the depth of a 3D texture level
     * @param level texture level.
     * @return Depth in texel of the specified \p level, clamped to 1.
     * @attention If this texture is using Sampler::SAMPLER_EXTERNAL, the dimension
     * of the texture are unknown and this method always returns whatever was set on the Builder.
     */
    size_t getDepth(size_t level = BASE_LEVEL) const noexcept;

    /**
     * Returns the maximum number of levels this texture can have.
     * @return maximum number of levels this texture can have.
     * @attention If this texture is using Sampler::SAMPLER_EXTERNAL, the dimension
     * of the texture are unknown and this method always returns whatever was set on the Builder.
     */
    size_t getLevels() const noexcept;

    /**
     * Return this texture Sampler as set by Builder::sampler().
     * @return this texture Sampler as set by Builder::sampler()
     */
    Sampler getTarget() const noexcept;

    /**
     * Return this texture InternalFormat as set by Builder::format().
     * @return this texture InternalFormat as set by Builder::format().
     */
    InternalFormat getFormat() const noexcept;

    /**
     * Updates a sub-image of a 3D texture or 2D texture array for a level. Cubemaps are treated
     * like a 2D array of six layers.
     *
     * @param engine    Engine this texture is associated to.
     * @param level     Level to set the image for.
     * @param xoffset   Left offset of the sub-region to update.
     * @param yoffset   Bottom offset of the sub-region to update.
     * @param zoffset   Depth offset of the sub-region to update.
     * @param width     Width of the sub-region to update.
     * @param height    Height of the sub-region to update.
     * @param depth     Depth of the sub-region to update.
     * @param buffer    Client-side buffer containing the image to set.
     *
     * @attention \p engine must be the instance passed to Builder::build()
     * @attention \p level must be less than getLevels().
     * @attention \p buffer's Texture::Format must match that of getFormat().
     * @attention This Texture instance must use Sampler::SAMPLER_3D, Sampler::SAMPLER_2D_ARRAY
     *             or Sampler::SAMPLER_CUBEMAP.
     *
     * @see Builder::sampler()
     */
    void setImage(Engine& engine, size_t level,
            uint32_t xoffset, uint32_t yoffset, uint32_t zoffset,
            uint32_t width, uint32_t height, uint32_t depth,
            PixelBufferDescriptor&& buffer) const;

    /**
     * inline helper to update a 2D texture
     *
     * @see setImage(Engine& engine, size_t level,
     *              uint32_t xoffset, uint32_t yoffset, uint32_t zoffset,
     *              uint32_t width, uint32_t height, uint32_t depth,
     *              PixelBufferDescriptor&& buffer)
     */
    void setImage(Engine& engine, size_t level, PixelBufferDescriptor&& buffer) const {
        setImage(engine, level, 0, 0, 0,
            uint32_t(getWidth(level)), uint32_t(getHeight(level)), 1, std::move(buffer));
    }

    /**
     * inline helper to update a 2D texture
     *
     * @see setImage(Engine& engine, size_t level,
     *              uint32_t xoffset, uint32_t yoffset, uint32_t zoffset,
     *              uint32_t width, uint32_t height, uint32_t depth,
     *              PixelBufferDescriptor&& buffer)
     */
    void setImage(Engine& engine, size_t level,
            uint32_t xoffset, uint32_t yoffset, uint32_t width, uint32_t height,
            PixelBufferDescriptor&& buffer) const {
        setImage(engine, level, xoffset, yoffset, 0, width, height, 1, std::move(buffer));
    }

    /**
     * Specify all six images of a cube map level.
     *
     * This method follows exactly the OpenGL conventions.
     *
     * @param engine        Engine this texture is associated to.
     * @param level         Level to set the image for.
     * @param buffer        Client-side buffer containing the images to set.
     * @param faceOffsets   Offsets in bytes into \p buffer for all six images. The offsets
     *                      are specified in the following order: +x, -x, +y, -y, +z, -z
     *
     * @attention \p engine must be the instance passed to Builder::build()
     * @attention \p level must be less than getLevels().
     * @attention \p buffer's Texture::Format must match that of getFormat().
     * @attention This Texture instance must use Sampler::SAMPLER_CUBEMAP or it has no effect
     *
     * @see Texture::CubemapFace, Builder::sampler()
     *
     * @deprecated Instead, use setImage(Engine& engine, size_t level,
     *              uint32_t xoffset, uint32_t yoffset, uint32_t zoffset,
     *              uint32_t width, uint32_t height, uint32_t depth,
     *              PixelBufferDescriptor&& buffer)
     */
    UTILS_DEPRECATED
    void setImage(Engine& engine, size_t level,
            PixelBufferDescriptor&& buffer, const FaceOffsets& faceOffsets) const;


    /**
     * Specify the external image to associate with this Texture. Typically, the external
     * image is OS specific, and can be a video or camera frame.
     * There are many restrictions when using an external image as a texture, such as:
     *   - only the level of detail (lod) 0 can be specified
     *   - only nearest or linear filtering is supported
     *   - the size and format of the texture is defined by the external image
     *   - only the CLAMP_TO_EDGE wrap mode is supported
     *
     * @param engine        Engine this texture is associated to.
     * @param image         An opaque handle to a platform specific image. It must be created using Platform
     *                      specific APIs. For example PlatformEGL::createExternalImage(EGLImageKHR eglImage)
     *
     * @see PlatformEGL::createExternalImage
     * @see PlatformEGLAndroid::createExternalImage
     * @see PlatformCocoaGL::createExternalImage
     * @see PlatformCocoaTouchGL::createExternalImage
     */
    void setExternalImage(Engine& engine, ExternalImageHandleRef image) noexcept;

    /**
     * Specify the external image to associate with this Texture. Typically, the external
     * image is OS specific, and can be a video or camera frame.
     * There are many restrictions when using an external image as a texture, such as:
     *   - only the level of detail (lod) 0 can be specified
     *   - only nearest or linear filtering is supported
     *   - the size and format of the texture is defined by the external image
     *   - only the CLAMP_TO_EDGE wrap mode is supported
     *
     * @param engine        Engine this texture is associated to.
     * @param image         An opaque handle to a platform specific image. Supported types are
     *                      eglImageOES on Android and CVPixelBufferRef on iOS.
     *
     *                      On iOS the following pixel formats are supported:
     *                        - kCVPixelFormatType_32BGRA
     *                        - kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
     *
     * @attention \p engine must be the instance passed to Builder::build()
     * @attention This Texture instance must use Sampler::SAMPLER_EXTERNAL or it has no effect
     *
     * @see Builder::sampler()
     *
     * @deprecated Instead, use setExternalImage(Engine& engine, ExternalImageHandleRef image)
     */
    UTILS_DEPRECATED
    void setExternalImage(Engine& engine, void* UTILS_NONNULL image) noexcept;

    /**
     * Specify the external image and plane to associate with this Texture. Typically, the external
     * image is OS specific, and can be a video or camera frame. When using this method, the
     * external image must be a planar type (such as a YUV camera frame). The plane parameter
     * selects which image plane is bound to this texture.
     *
     * A single external image can be bound to different Filament textures, with each texture
     * associated with a separate plane:
     *
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     * textureA->setExternalImage(engine, image, 0);
     * textureB->setExternalImage(engine, image, 1);
     * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
     *
     * There are many restrictions when using an external image as a texture, such as:
     *   - only the level of detail (lod) 0 can be specified
     *   - only nearest or linear filtering is supported
     *   - the size and format of the texture is defined by the external image
     *   - only the CLAMP_TO_EDGE wrap mode is supported
     *
     * @param engine        Engine this texture is associated to.
     * @param image         An opaque handle to a platform specific image. Supported types are
     *                      eglImageOES on Android and CVPixelBufferRef on iOS.
     * @param plane         The plane index of the external image to associate with this texture.
     *
     *                      This method is only meaningful on iOS with
     *                      kCVPixelFormatType_420YpCbCr8BiPlanarFullRange images. On platforms
     *                      other than iOS, this method is a no-op.
     */
    void setExternalImage(Engine& engine, void* UTILS_NONNULL image, size_t plane) noexcept;

    /**
     * Specify the external stream to associate with this Texture. Typically, the external
     * stream is OS specific, and can be a video or camera stream.
     * There are many restrictions when using an external stream as a texture, such as:
     *   - only the level of detail (lod) 0 can be specified
     *   - only nearest or linear filtering is supported
     *   - the size and format of the texture is defined by the external stream
     *
     * @param engine        Engine this texture is associated to.
     * @param stream        A Stream object
     *
     * @attention \p engine must be the instance passed to Builder::build()
     * @attention This Texture instance must use Sampler::SAMPLER_EXTERNAL, or it has no effect
     *
     * @see Builder::sampler(), Stream
     *
     */
    void setExternalStream(Engine& engine, Stream* UTILS_NULLABLE stream) noexcept;

    /**
     * Generates all the mipmap levels automatically. This requires the texture to have a
     * color-renderable format and usage set to BLIT_SRC | BLIT_DST. If unspecified,
     * usage bits are set automatically.
     *
     * @param engine        Engine this texture is associated to.
     *
     * @attention \p engine must be the instance passed to Builder::build()
     * @attention This Texture instance must NOT use SamplerType::SAMPLER_3D or it has no effect
     */
    void generateMipmaps(Engine& engine) const noexcept;

    /** @deprecated */
    struct FaceOffsets {
        using size_type = size_t;
        union {
            struct {
                size_type px;   //!< +x face offset in bytes
                size_type nx;   //!< -x face offset in bytes
                size_type py;   //!< +y face offset in bytes
                size_type ny;   //!< -y face offset in bytes
                size_type pz;   //!< +z face offset in bytes
                size_type nz;   //!< -z face offset in bytes
            };
            size_type offsets[6];
        };
        size_type  operator[](size_t n) const noexcept { return offsets[n]; }
        size_type& operator[](size_t n) { return offsets[n]; }
        FaceOffsets() noexcept = default;
        explicit FaceOffsets(size_type faceSize) noexcept {
            px = faceSize * 0;
            nx = faceSize * 1;
            py = faceSize * 2;
            ny = faceSize * 3;
            pz = faceSize * 4;
            nz = faceSize * 5;
        }
        FaceOffsets(const FaceOffsets& rhs) noexcept {
            px = rhs.px;
            nx = rhs.nx;
            py = rhs.py;
            ny = rhs.ny;
            pz = rhs.pz;
            nz = rhs.nz;
        }
        FaceOffsets& operator=(const FaceOffsets& rhs) noexcept {
            px = rhs.px;
            nx = rhs.nx;
            py = rhs.py;
            ny = rhs.ny;
            pz = rhs.pz;
            nz = rhs.nz;
            return *this;
        }
    };

protected:
    // prevent heap allocation
    ~Texture() = default;
};

} // namespace filament

#endif // TNT_FILAMENT_TEXTURE_H
