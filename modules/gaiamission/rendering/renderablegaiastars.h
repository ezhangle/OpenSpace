/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2018                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#ifndef __OPENSPACE_MODULE_GAIAMISSION___RENDERABLEGAIASTARS___H__
#define __OPENSPACE_MODULE_GAIAMISSION___RENDERABLEGAIASTARS___H__

#include <openspace/rendering/renderable.h>

#include <openspace/properties/stringproperty.h>
#include <openspace/properties/stringlistproperty.h>
#include <openspace/properties/scalar/floatproperty.h>
#include <openspace/properties/scalar/intproperty.h>

#include <ghoul/opengl/ghoul_gl.h>
#include <ghoul/opengl/uniformcache.h>

namespace ghoul::filesystem { class File; }
namespace ghoul::opengl {
    class ProgramObject;
    class Texture;
} // namespace ghoul::opengl

namespace openspace {

namespace documentation { struct Documentation; }

class RenderableGaiaStars : public Renderable {
public:
    explicit RenderableGaiaStars(const ghoul::Dictionary& dictionary);
    ~RenderableGaiaStars();

    void initializeGL() override;
    void deinitializeGL() override;

    bool isReady() const override;

    void render(const RenderData& data, RendererTasks& rendererTask) override;
    void update(const UpdateData& data) override;

    static documentation::Documentation Documentation();

private:
    void createDataSlice();
    bool loadData();
    bool readFitsFile();
    bool loadCachedFile(const std::string& file);
    bool saveCachedFile(const std::string& file) const;

    properties::StringProperty _fitsFilePath;
    std::unique_ptr<ghoul::filesystem::File> _fitsFile;
    bool _dataIsDirty;

    properties::StringProperty _pointSpreadFunctionTexturePath;
    std::unique_ptr<ghoul::opengl::Texture> _pointSpreadFunctionTexture;
    std::unique_ptr<ghoul::filesystem::File> _pointSpreadFunctionFile;
    bool _pointSpreadFunctionTextureIsDirty;

    properties::StringProperty _colorTexturePath;
    std::unique_ptr<ghoul::opengl::Texture> _colorTexture;
    std::unique_ptr<ghoul::filesystem::File> _colorTextureFile;
    bool _colorTextureIsDirty;

    properties::FloatProperty _alphaValue;
    properties::FloatProperty _scaleFactor;
    properties::FloatProperty _minBillboardSize;

    properties::IntProperty _firstRow;
    properties::IntProperty _lastRow;
    properties::StringListProperty _columnNamesList;
    std::vector<std::string> _columnNames;

    std::unique_ptr<ghoul::opengl::ProgramObject> _program;
    UniformCache(view, projection, alphaValue, scaleFactor,
        minBillboardSize, screenSize, psfTexture, time, colorTexture, scaling) _uniformCache;

    std::vector<float> _slicedData;
    std::vector<float> _fullData;
    size_t _nValuesPerStar;

    GLuint _vao;
    GLuint _vbo;
};

} // namespace openspace

#endif // __OPENSPACE_MODULE_GAIAMISSION___RENDERABLEGAIASTARS___H__
