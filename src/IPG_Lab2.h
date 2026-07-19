#pragma once

#include <koral.h>

#include <vector>

class IPG_Lab2 final : public kor::Scene
{
public:
    void Initialize() override;
    void Update() override;
    void Render(kor::CommandBuffer& commandBuffer) override;
    void RenderUI(ImGuiContext* context) override;

    // Triangles are authored in this fixed canvas space rather than in window pixels, so
    // resizing the window scales the picture (the blit stretches it) instead of moving every
    // vertex relative to the image it was placed against.
    static constexpr glm::uvec2 CANVAS_SIZE { 1280, 720 };

    // The buffers are allocated once at these capacities and only ever rewritten, because a
    // Buffer cannot be resized - and reallocating one would invalidate the descriptor set
    // pointing at it every time the user added a vertex.
    static constexpr int MAX_VERTICES = 512;
    static constexpr int MAX_TRIANGLES = 256;

    struct Vertex {
        alignas(16) glm::vec3 position;  ///< xy in canvas pixels, z is depth in [0, 1]
        alignas(16) glm::vec3 color;     ///< rgb, each channel in [0, 1]
    };

    // Matches StructuredBuffer<Vertex> in rasterizer.slang: float3 is 16-byte aligned there,
    // so the alignas above is what makes the two agree.
    static_assert(sizeof(Vertex) == 32 && offsetof(Vertex, color) == 16);

    // A triangle is three indices into `vertices`, not three vertices of its own. Vertices are
    // therefore shared: moving one moves every triangle that references it, which is the whole
    // point of describing the mesh this way.
    using Triangle = glm::uvec3;

    // Uploaded by memcpy'ing the vector into a StructuredBuffer<uint>, so the triples have to
    // be three tightly packed uints with no padding between them.
    static_assert(sizeof(Triangle) == 3 * sizeof(glm::uint));

    kor::Resource<kor::ComputePipeline> pipeline;

    kor::Resource<kor::Buffer> vertexBuffer;
    kor::Resource<kor::Buffer> indexBuffer;

    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;

    kor::Resource<kor::Image> colorImage;
    kor::Resource<kor::Image> depthImage;

    kor::Resource<kor::ImageView> colorImageView;
    kor::Resource<kor::ImageView> depthImageView;

    kor::Resource<kor::DescriptorSet> descriptorSet;

private:
    void UploadMesh();

    // One selection across both lists: a vertex and a triangle are edited in the same
    // Properties panel, so only one of them can own it at a time.
    enum class Selection { None, Vertex, Triangle };

    Selection selection = Selection::None;
    int selectedIndex = -1;

    // Set by the UI, consumed by Update: the buffers are rewritten once per frame at most,
    // rather than on every individual drag callback within a frame.
    bool meshDirty = false;

    // Read from the docked window a frame after it exists, so the default layout can be
    // built against whatever node the engine's dockspace put it in.
    ImGuiID meshDockId = 0;
    ImGuiID propertiesDockId = 0;
    bool dockLayoutBuilt = false;
};
