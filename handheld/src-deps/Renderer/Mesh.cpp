#include "Mesh.h"
#include "TexturePtr.h"
#include "hal/interface/RenderContextImmediate.h"
#include "materialptr.h"
#include <vector>
#include <glad/glad.h>
namespace mce
{
    uint64_t Mesh::sGlGenCalls = 0;
    uint64_t Mesh::sGlDeleteCalls = 0;
    uint64_t Mesh::sGlPoolHits = 0;
    unsigned int Mesh::sLastTerrainVAO = 0;

    namespace {
        constexpr size_t MAX_POOLED_GL_HANDLES = 1024;
        std::vector<unsigned int> sFreeVAOs;
        std::vector<unsigned int> sFreeVBOs;
        std::vector<unsigned int> sFreeEBOs;
        bool sPoolActive = true;

        unsigned int acquireGlHandle(std::vector<unsigned int>& pool, bool isVao) {
            if (!pool.empty()) {
                unsigned int id = pool.back();
                pool.pop_back();
                ++Mesh::sGlPoolHits;
                return id;
            }
            unsigned int id = 0;
            if (isVao) {
                glGenVertexArrays(1, &id);
            } else {
                glGenBuffers(1, &id);
            }
            ++Mesh::sGlGenCalls;
            return id;
        }

        void recycleGlHandle(unsigned int& id, std::vector<unsigned int>& pool, bool isVao) {
            constexpr unsigned int invalidId = static_cast<unsigned int>(-1);
            if (id == invalidId) {
                return;
            }
            if (sPoolActive && pool.size() < MAX_POOLED_GL_HANDLES) {
                pool.push_back(id);
            } else if (sPoolActive) {
                if (isVao) {
                    glDeleteVertexArrays(1, &id);
                } else {
                    glDeleteBuffers(1, &id);
                }
                ++Mesh::sGlDeleteCalls;
            }
            id = invalidId;
        }
    }

    int fieldSizes[] =
    {
        sizeof(float) * 3,
        sizeof(unsigned int),
        sizeof(int),
        sizeof(unsigned short) * 2,
        sizeof(unsigned short) * 2,
        sizeof(unsigned short) * 2,
    };

    int fieldTypes[] =
    {
        GL_FLOAT,
        GL_UNSIGNED_BYTE,
        GL_UNSIGNED_INT,
        GL_UNSIGNED_SHORT,
        GL_UNSIGNED_SHORT,
        GL_UNSIGNED_SHORT,
    };

    int primitiveConversions[] =
    {
        0,
        GL_QUADS,
        GL_TRIANGLES,
        GL_TRIANGLE_STRIP,
        GL_LINES,
        GL_LINE_STRIP
    };

    bool doNormalize[] =
    {
        false,
        true,
        false,
        true,//we want our uvs to be converted back to 0-1
        true,
        true,
    };

    int fieldAmounts[] =
    {
        3,
        4,
        1,
        2,
        2,
        2,
    };

    const char* vertexFormatNames[] =
    {
        "POSITION",
        "COLOR",
        "NORMAL",
        "TEXCOORD_0",
        "TEXCOORD_1",
        "TEXCOORD_2",
    };

    VertexFormat VertexFormat::EMPTY;

    void VertexFormat::uploadToVAO(unsigned int VAO)
    {
            glBindVertexArray(VAO);

            int offs = 0;

            int vSize = getVertexSize();

            for (int i = 0; i < (int)VertexField::NumVertexFields; i++)
            {
                if (useFields[i])
                {
                    glEnableVertexAttribArray(i);
                    //quick-ish hack
                    if(fieldTypes[i] == GL_FLOAT || fieldTypes[i] == GL_HALF_FLOAT || doNormalize[i])
                    glVertexAttribPointer(i, fieldAmounts[i], fieldTypes[i], doNormalize[i], vSize, (void*)offs);
                    else
                        glVertexAttribIPointer(i, fieldAmounts[i], fieldTypes[i], vSize, (void*)offs);
                    offs += fieldSizes[i];
                }
                else
                {
                    glDisableVertexAttribArray(i);
                }
            }
            glBindVertexArray(0);
    }

	void Mesh::upload()
	{
        VAO = acquireGlHandle(sFreeVAOs, true);
        glBindVertexArray(VAO);

        VBO = acquireGlHandle(sFreeVBOs, false);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * vertexSize, byteData, GL_STATIC_DRAW);

        if (indexCount * indexSize > 0)
        {
            EBO = acquireGlHandle(sFreeEBOs, false);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * indexSize, byteData + (vertexCount * vertexSize), GL_STATIC_DRAW);
        }
        else
        {
            // Clear any prior EBO binding on a recycled VAO until an index buffer is attached
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        }

        vf.uploadToVAO(VAO);//apparently this doesnt work if you put it at the start (setting up the VAO also binds the vbo etc to it)

        glBindVertexArray(0);
	}

    void Mesh::render()
    {
        if (VBO == -1)
            upload();

        glBindVertexArray(VAO);
       

        if (useMode == IndexBufferUsageMode::Normal && indexCount * indexSize > 0)
        {
            int realMode = mode == PrimitiveMode::QuadList ? GL_TRIANGLES : primitiveConversions[(int)mode];
            glDrawElements(realMode, indexCount, indexSize == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, 0);
        }
        else
        {
            GLenum err;
            while ((err = glGetError()) != GL_NO_ERROR)
                printf("Before draw: %x\n", err);

            glDrawArrays(primitiveConversions[(int)mode], 0, vertexCount);

            while ((err = glGetError()) != GL_NO_ERROR)
                printf("After draw: %x\n", err);

           // glDrawArrays(primitiveConversions[(int)mode], 0, vertexCount);
        }
        glBindVertexArray(0);
    }

    void Mesh::_move(Mesh & other)
    {
        VAO = other.VAO;
        VBO = other.VBO;
        EBO = other.EBO;
        other.VAO = -1;
        other.VBO = -1;
        other.EBO = -1;
        vf = other.vf;
        vertexCount = other.vertexCount;
        vertexSize = other.vertexSize;
        indexCount = other.indexCount;
        indexSize = other.indexSize;
        mode = other.mode;
        byteData = other.byteData;
        other.vertexCount = 0;
        other.vertexSize = 0;
        other.indexCount = 0;
        other.indexSize = 0;
        other.byteData = nullptr;
    }

    Mesh& Mesh::operator=(Mesh&& lll)
    {
        if (this != &lll) {
            reset();
            _move(lll);
        }
        return *this;
    }

    void Mesh::render(const mce::MaterialPtr& mat)
    {
        mat->activate();
        render();
    }

    void Mesh::render(const mce::MaterialPtr& mat, const mce::TexturePtr& text1)
    {
        if ((*text1) != nullptr)
        text1->bindTexture(RenderContextImmediate::get(), 0, 0);
        render(mat);
    }

    void Mesh::render(const mce::MaterialPtr& mat, const mce::TexturePtr& text, const mce::TexturePtr& text2)
    {
        if ((*text2) != nullptr)
        text2->bindTexture(RenderContextImmediate::get(), 1, 0);
        render(mat,text);
    }

    void Mesh::render(const mce::MaterialPtr& mat, const mce::TexturePtr& text1, const mce::TexturePtr&text2, const mce::TexturePtr&text3, int start, int count)
    {
        if((*text3) != nullptr)
        text3->bindTexture(RenderContextImmediate::get(), 2, 0);
        if ((*text2) != nullptr)
        text2->bindTexture(RenderContextImmediate::get(), 1, 0);
        if ((*text1) != nullptr)
        text1->bindTexture(RenderContextImmediate::get(), 0, 0);
        mat->activate();
        if (VBO == -1)
            upload();

        glBindVertexArray(VAO);


        if (useMode == IndexBufferUsageMode::Normal && indexCount * indexSize > 0)
        {
            //printf("start %i count %i\n", start, count);
            int realMode = mode == PrimitiveMode::QuadList ? GL_TRIANGLES : primitiveConversions[(int)mode];//hack because for some reason sorting changes this to tris not gl_quad (maybe non-sorted wasnt updated from immediate mode gl?)
            glDrawElements(realMode, count, indexSize == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, (void *)(start*indexSize));//apparently this is in bytes not in units of indices
        }
        else
        {
            GLenum err;
            while ((err = glGetError()) != GL_NO_ERROR)
                printf("Before draw: %x\n", err);

            

            glDrawArrays(primitiveConversions[(int)mode], 0, vertexCount);

            while ((err = glGetError()) != GL_NO_ERROR)
                printf("After draw: %x\n", err);

            // glDrawArrays(primitiveConversions[(int)mode], 0, vertexCount);
        }
        glBindVertexArray(0);
    }

    void Mesh::renderRangeNoUnbind(int start, int count)
    {
        if (VBO == -1) {
            upload();
            sLastTerrainVAO = 0;
        }

        if (sLastTerrainVAO != VAO) {
            glBindVertexArray(VAO);
            sLastTerrainVAO = VAO;
        }
        if (useMode == IndexBufferUsageMode::Normal && indexCount * indexSize > 0)
        {
            int realMode = mode == PrimitiveMode::QuadList ? GL_TRIANGLES : primitiveConversions[(int)mode];
            glDrawElements(realMode, count, indexSize == 4 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, (void *)(start * indexSize));
        }
        else
        {
            glDrawArrays(primitiveConversions[(int)mode], 0, vertexCount);
        }
    }

    void Mesh::reset()
    {
        if (sLastTerrainVAO == VAO) {
            sLastTerrainVAO = 0;
        }
        recycleGlHandle(VAO, sFreeVAOs, true);
        recycleGlHandle(VBO, sFreeVBOs, false);
        recycleGlHandle(EBO, sFreeEBOs, false);

        vf = VertexFormat::EMPTY;
        vertexCount = 0;
        vertexSize = 0;
        indexCount = 0;
        indexSize = 0;
        mode = PrimitiveMode::QuadList;
        byteData = nullptr;
    }

    void Mesh::shutdownPool()
    {
        if (!sPoolActive) {
            return;
        }
        sPoolActive = false;
        if (!sFreeVAOs.empty()) {
            glDeleteVertexArrays(static_cast<GLsizei>(sFreeVAOs.size()), sFreeVAOs.data());
            sGlDeleteCalls += sFreeVAOs.size();
            sFreeVAOs.clear();
        }
        if (!sFreeVBOs.empty()) {
            glDeleteBuffers(static_cast<GLsizei>(sFreeVBOs.size()), sFreeVBOs.data());
            sGlDeleteCalls += sFreeVBOs.size();
            sFreeVBOs.clear();
        }
        if (!sFreeEBOs.empty()) {
            glDeleteBuffers(static_cast<GLsizei>(sFreeEBOs.size()), sFreeEBOs.data());
            sGlDeleteCalls += sFreeEBOs.size();
            sFreeEBOs.clear();
        }
    }

    Mesh::~Mesh()
    {
        reset();
    }


    void Mesh::loadIndexBuffer(uint32_t _indexSize, void const* data, unsigned int count)
    {
        constexpr unsigned int invalidId = static_cast<unsigned int>(-1);
        if (VBO == invalidId)
        {
            upload();
        }
        indexSize = _indexSize;
        indexCount = count;
        glBindVertexArray(VAO);
        if (EBO == invalidId)
        {
            EBO = acquireGlHandle(sFreeEBOs, false);
        }
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            count * indexSize,
            data,
            GL_STATIC_DRAW
        );
        glBindVertexArray(0);
    }

};